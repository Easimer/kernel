#include "common.h"
#include "volumes.h"
#include "disk.h"
#include "utils.h"
#include "logging.h"
#include "interrupts.h"
#include "syscalls.h"

#define MAX_VOLUMES (128)
#define MAX_FILESYSTEMS (8)

struct Filesystem {
    Filesystem_Descriptor* desc;
    void* user;
};

struct Volume_State {
    Volume_Descriptor desc;
    Filesystem filesystem;
};

static Volume_State gaVolumes[MAX_VOLUMES];
static u32 giVolumesLastIndex = 0;
static Filesystem_Descriptor* gaFilesystems[MAX_FILESYSTEMS];
static u32 giFilesystemsLastIndex = 0;

Volume_Handle Volume_Register(const Volume_Descriptor* vol) {
    Volume_Handle ret;
    ASSERT(giVolumesLastIndex < MAX_VOLUMES);

    auto& S = gaVolumes[giVolumesLastIndex];
    S.desc = *vol;
    S.filesystem.desc = NULL;
    S.filesystem.user = NULL;
    ret = giVolumesLastIndex;

    logprintf("New volume registered: disk %d start %x size %x\n", vol->disk, vol->offset, vol->length);

    giVolumesLastIndex++;

    return ret;
}

Filesystem* Volume_Get_Filesystem(Volume_Handle vol) {
    Filesystem* ret = NULL;

    if(vol < giVolumesLastIndex) {
        ret = &gaVolumes[vol].filesystem;
    }

    return ret;
}

s32 Volume_Read_Blocks(Volume_Handle vol, void* buffer, u32 offset, u32 count) {
    s32 ret = -1;

    if(vol < giVolumesLastIndex && buffer) {
        if(count > 0) {
            auto& V = gaVolumes[vol];
            auto disk = V.desc.disk;
            if(offset < V.desc.length && offset + count < V.desc.length) {
                ret = Disk_Read_Blocks(disk, buffer, count, offset + V.desc.offset);
            } else {
                logprintf("VolMan: read out of range: %x + 0:%x\n", offset, count);
            }
            ASSERT(offset < V.desc.length && offset + count < V.desc.length);
        } else {
            ret = 0;
        }
    }

    return ret;
}

s32 Volume_Write_Blocks(Volume_Handle vol, const void* buffer, u32 offset, u32 count) {
    s32 ret = -1;

    ASSERT(vol < giVolumesLastIndex);
    ASSERT(buffer);

    if(vol < giVolumesLastIndex && buffer) {
        if(count > 0) {
            auto& V = gaVolumes[vol];
            auto disk = V.desc.disk;
            
            if(offset < V.desc.length && offset + count < V.desc.length) {
                ret = Disk_Write_Blocks(disk, buffer, count, offset + V.desc.offset);
            } else {
                logprintf("VolMan: writing out of range: %x + 0:%x\n", offset, count);
            }
            ASSERT(offset < V.desc.length && offset + count < V.desc.length);
        } else {
            ret = 0;
        }
    }

    return ret;
}

static void Volume_Detect_Filesystems() {
    for(u32 vol = 0; vol < giVolumesLastIndex; vol++) {
        bool OK = false;
        auto& V = gaVolumes[vol];
        for(u32 fs = 0; fs < giFilesystemsLastIndex && !OK; fs++) {
            auto FS = gaFilesystems[fs];

            if(FS->Probe) {
                void* user = NULL;
                if(FS->Probe(vol, &user)) {
                    V.filesystem.desc = FS;
                    V.filesystem.user = user;
                    OK = true;
                    logprintf("Mounting volume #%d as '%s'!\n", vol, FS->Name);
                }
            }
        }

        if(!OK) {
            logprintf("Couldn't find filesystem driver for volume %d\n", vol);
        }
    }
}

void Filesystem_Register_Filesystem(Filesystem_Register init) {
    if(giFilesystemsLastIndex < MAX_FILESYSTEMS) {
        gaFilesystems[giFilesystemsLastIndex] = init();
        giFilesystemsLastIndex++;
    }
}

// -------------------------------------------------------------------

#define MAX_OPEN_FILES (64)

struct File_Handle_Mapping {
    bool used = false;
    Filesystem_File_Handle fd;
    Volume_Handle vol;
};

static File_Handle_Mapping gaFDMap[MAX_OPEN_FILES];

int File_Open(Volume_Handle volume, const char* path, mode_t flags) {
    int ret = -1;

    logprintf("File_Open\n");

    if(volume < giVolumesLastIndex && path && flags > 0) {
        int ch = -1;
        // Find a free handle
        for(int i = 0; i < MAX_OPEN_FILES && ch == -1; i++) {
            if(!gaFDMap[i].used) {
                ch = i;
            }
        }
        if(ch != -1) {
            auto& V = gaVolumes[volume];
            auto& FS = V.filesystem;
            auto handle = FS.desc->Open(FS.user, path, flags);
            if(handle != -1) {
                gaFDMap[ch].used = true;
                gaFDMap[ch].fd = handle;
                gaFDMap[ch].vol = volume;
                ret = ch;
            }
        } else {
            logprintf("File handles exhausted\n");
        }
    } else {
        logprintf("Invalid volume (%d/%d), NULL path (%x) or no flags (%x)\n", volume, giVolumesLastIndex, path, flags);
    }

    return ret;
}

void File_Close(int fd) {
    if(fd < MAX_OPEN_FILES) {
        if(gaFDMap[fd].used) {
            auto& f = gaFDMap[fd];
            ASSERT(f.vol < giVolumesLastIndex);
            if(f.vol < giVolumesLastIndex) {
                auto& V = gaVolumes[f.vol];
                V.filesystem.desc->Close(V.filesystem.user, f.fd);
            }
            f.used = false;
        }
    }
}

int File_Read(void* ptr, u32 size, u32 nmemb, int fd) {
    int ret = -1;

    if(ptr && size > 0 && fd >= 0 && fd < MAX_OPEN_FILES) {
        if(nmemb > 0) {
            if(gaFDMap[fd].used) {
                auto& f = gaFDMap[fd];
                ASSERT(f.vol < giVolumesLastIndex);
                auto& V = gaVolumes[f.vol];
                s32 res = V.filesystem.desc->Read(V.filesystem.user, f.fd, ptr, size * nmemb);
                if(res > 0) {
                    ASSERT(res % size == 0);
                    ret = res / size;
                } else {
                    logprintf("RD ERROR\n");
                }
            }
        } else {
            ret = 0;
        }
    } else {
        // Bad argument(s)
    }

    return ret;
}

int File_Write(const void* ptr, u32 size, u32 nmemb, int fd) {
    int ret = -1;

    if(ptr && size > 0 && fd >= 0 && fd < MAX_OPEN_FILES) {
        if(nmemb > 0) {
            if(gaFDMap[fd].used) {
                auto& f = gaFDMap[fd];
                ASSERT(f.vol < giVolumesLastIndex);
                auto& V = gaVolumes[f.vol];
                s32 res = V.filesystem.desc->Write(V.filesystem.user, f.fd, ptr, size * nmemb);
                if(res > 0) {
                    ASSERT(res % size == 0);
                    ret = res / size;
                } else {
                    logprintf("WR ERROR\n");
                }
            }
        } else {
            ret = 0;
        }
    } else {
        // Bad argument(s)
    }

    return ret;
}

void File_Seek(int fd, s32 offset, whence_t whence) {
    if(fd >= 0 && fd < MAX_OPEN_FILES) {
        if(gaFDMap[fd].used) {
            auto& f = gaFDMap[fd];
            ASSERT(f.vol < giVolumesLastIndex);
            auto& V = gaVolumes[f.vol];
            V.filesystem.desc->Seek(V.filesystem.user, f.fd, whence, offset);
        } else {
            //logprintf("FSEEK:: free fd\n");
        }
    } else {
        //logprintf("FSEEK:: bad fd\n");
    }
}

int File_Tell(int fd) {
    int ret = -1;

    if(fd >= 0 && fd < MAX_OPEN_FILES) {
        if(gaFDMap[fd].used) {
            auto& f = gaFDMap[fd];
            ASSERT(f.vol < giVolumesLastIndex);
            auto& V = gaVolumes[f.vol];
            if(V.filesystem.desc->Seek) {
                ret = V.filesystem.desc->Tell(V.filesystem.user, f.fd);
            }
        }
    }

    return ret;
}

void Sync(Volume_Handle volume) {
    ASSERT(volume < giVolumesLastIndex);
    auto& V = gaVolumes[volume];
    if(V.filesystem.desc->Sync) {
        logprintf("sync=%x user=%x\n", V.filesystem.desc->Sync, V.filesystem.user);
        V.filesystem.desc->Sync(V.filesystem.user);
    }
}

int File_EOF(int fd) {
    int ret = -1;
    
    if(fd >= 0 && fd < MAX_OPEN_FILES) {
        if(gaFDMap[fd].used) {
            auto& f = gaFDMap[fd];
            ASSERT(f.vol < giVolumesLastIndex);
            auto& V = gaVolumes[f.vol];
            ret = V.filesystem.desc->EOF(V.filesystem.user, f.fd);
        }
    }

    return ret;
}

static void SC_Handler(Registers* regs) {
    int res = regs->eax;
    switch(regs->eax) {
        case SYSCALL_OPEN:
        res = File_Open(regs->ebx, (char*)regs->edx, regs->ecx);
        break;
        case SYSCALL_CLOSE:
        File_Close(regs->ebx);
        break;
        case SYSCALL_READ:
        res = File_Read((void*)regs->edi, regs->ebx, regs->ecx, regs->edx);
        break;
        case SYSCALL_WRITE:
        res = File_Write((void*)regs->esi, regs->ebx, regs->ecx, regs->edx);
        break;
        case SYSCALL_SEEK:
        res = 0;
        File_Seek(regs->edx, regs->ecx, (whence_t)regs->ebx);
        break;
        case SYSCALL_TELL:
        res = File_Tell(regs->edx);
        break;
    }
    regs->eax = res;
}

void Volume_Init() {
    // Detect filesystems
    Volume_Detect_Filesystems();

    // Register syscalls
    RegisterSyscallHandler(SYSCALL_OPEN, SC_Handler);
    RegisterSyscallHandler(SYSCALL_CLOSE, SC_Handler);
    RegisterSyscallHandler(SYSCALL_READ, SC_Handler);
    RegisterSyscallHandler(SYSCALL_WRITE, SC_Handler);
    RegisterSyscallHandler(SYSCALL_SEEK, SC_Handler);
    RegisterSyscallHandler(SYSCALL_TELL, SC_Handler);
}