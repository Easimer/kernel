#include "common.h"
#include "volumes.h"
#include "memory.h"
#include "utils.h"

#include "uart.h"
#include "pc_vga.h"

enum Filetype {
    FT_Invalid = 0,
    FT_Null, // /dev/null
    FT_Zero, // /dev/zero
    FT_Serial, // COM1-4
    FT_VGA, // VGA framebuffer
    FT_Memory, // Memory access
};

#define MAX_OPEN_FILES (16)

struct File_State {
    Filetype type;
    union {
        struct Invalid {} invalid;
        struct Null {} null;
        struct Zero {} zero;
        struct Serial {
            u32 port_id; // [0, 4]
        } serial;
        struct VGA {
            u32 offset;
        } vga;
        struct Memory {
            void* addr;
            bool write;
        } mem;
    } state;
};

struct DevFS_State {
    File_State files[MAX_OPEN_FILES];
};

static s32 FindFreeHandle(DevFS_State* fs) {
    s32 ret = -1;
    for(s32 i = 0; i < MAX_OPEN_FILES && ret == -1; i++) {
        if(fs->files[i].type == FT_Invalid) {
            ret = i;
        }
    }
    return ret;
}

static Filesystem_File_Handle FS_Open(void* user, const char* path, mode_t flags) {
    auto fs = (DevFS_State*)user;
    Filesystem_File_Handle ret = -1;
    ASSERT(fs);
    
    if(path && flags > 0) {
        if(strncmp("tty", path, 3)) {
            char idch = path[3];
            u32 id = (idch - '0');
            if(id < 4) {
                ret = Filesystem_File_Handle(FindFreeHandle(fs));
                if(ret != -1) {
                    auto& F = fs->files[ret];
                    F.type = FT_Serial;
                    F.state.serial.port_id = id;
                }
            }
        } else if(strcmp("mem", path)) {
            ret = Filesystem_File_Handle(FindFreeHandle(fs));
            if(ret != -1) {
                auto& F = fs->files[ret];
                F.type = FT_Memory;
                F.state.mem.addr = NULL;
                F.state.mem.write = (flags & O_WRONLY) != 0;
            }
        } else if(strcmp("vga", path)) {
            ret = Filesystem_File_Handle(FindFreeHandle(fs));
            if(ret != -1) {
                auto& F = fs->files[ret];
                F.type = FT_VGA;
                F.state.vga.offset = 0;
            }
        } else if(strcmp("null", path)) {
            ret = Filesystem_File_Handle(FindFreeHandle(fs));
            if(ret != -1) {
                auto& F = fs->files[ret];
                F.type = FT_Null;
            }
        } else if(strcmp("zero", path)) {
            ret = Filesystem_File_Handle(FindFreeHandle(fs));
            if(ret != -1) {
                auto& F = fs->files[ret];
                F.type = FT_Zero;
            }
        }
    }

    return ret;
}

void FS_Close(void* user, Filesystem_File_Handle fd) {
    auto fs = (DevFS_State*)user;
    ASSERT(fs);

    if(fd >= 0 && fd < MAX_OPEN_FILES) {
        // Do cleanup
        switch(fs->files[fd].type) {
            case FT_Serial:
            // Flush serial port buffer
            UART_Flush(fs->files[fd].state.serial.port_id);
            break;
            default:
            // Nothing to do
            break;
        }

        fs->files[fd].type = FT_Invalid;
    }
}

static s32 FS_Read(void* user, Filesystem_File_Handle fd, void* dst, u32 bytes) {
    s32 ret = -1;
    auto fs = (DevFS_State*)user;
    ASSERT(fs);

    if(fd >= 0 && fd < MAX_OPEN_FILES && dst && bytes) {
        auto& F = fs->files[fd];

        switch(F.type) {
            case FT_Zero:
                memset(dst, 0, bytes);
                ret = bytes;
                break;
            case FT_Memory:
                // TODO: should we check if a GPF happened here or just let it crash?
                memcpy(dst, F.state.mem.addr, bytes);
                F.state.mem.addr = ((u8*)F.state.mem.addr + bytes);
                break;
            default:
                break;
        }
    }

    return ret;
}

static s32 FS_Write(void* user, Filesystem_File_Handle fd, const void* src, u32 bytes) {
    s32 ret = -1;
    auto fs = (DevFS_State*)user;
    ASSERT(fs);

    if(fd >= 0 && fd < MAX_OPEN_FILES && src && bytes) {
        auto& F = fs->files[fd];

        switch(F.type) {
            case FT_Memory:
                if(F.state.mem.write) {
                    memcpy(F.state.mem.addr, src, bytes);
                    F.state.mem.addr = ((u8*)F.state.mem.addr + bytes);
                }
                break;
            case FT_Null:
                ret = bytes;
                break;
            case FT_Serial:
            {
                auto port = F.state.serial.port_id;
                for(u32 i = 0; i < bytes; i++) {
                    UART_PutChar(port, ((u8*)src)[i]);
                }
                break;
            }
            case FT_VGA:
                for(u32 i = 0; i < bytes; i++) {
                    PCVGA_PutChar(((u8*)src)[i]);
                }
                break;
            default:
                break;
        }
    }

    return ret;
}

s32 FS_Tell(void* user, Filesystem_File_Handle fd) {
    s32 ret = -1;
    auto fs = (DevFS_State*)user;
    ASSERT(fs);

    if(fd >= 0 && fd < MAX_OPEN_FILES) {
        auto& F = fs->files[fd];

        switch(F.type) {
            case FT_Memory:
                ret = reinterpret_cast<s32>(F.state.mem.addr);
                break;
            case FT_Null:
            case FT_Zero:
            case FT_VGA:
                ret = 0;
                break;
            default:
                break;
        }
    }

    return ret;
}

s32 FS_Seek(void* user, Filesystem_File_Handle fd, whence_t whence, s32 position) {
    s32 ret = -1;
    auto fs = (DevFS_State*)user;
    ASSERT(fs);

    if(fd >= 0 && fd < MAX_OPEN_FILES) {
        auto& F = fs->files[fd];

        switch(F.type) {
            case FT_Memory:
                switch(whence) {
                    case whence_t::CUR:
                        F.state.mem.addr = (u8*)F.state.mem.addr + position;
                        break;
                    case whence_t::SET:
                        F.state.mem.addr = (void*)position;
                        break;
                    case whence_t::END:
                        F.state.mem.addr = (u8*)0xFFFFFFFF + position;
                        break;
                }
                break;
            case FT_Null:
            case FT_Zero:
                ret = 0;
                break;
            default:
                break;
        }
    }

    return ret;
}

int FS_EOF(void* user, Filesystem_File_Handle fd) {
    int ret = 0;
    auto fs = (DevFS_State*)user;
    ASSERT(fs);

    if(fd >= 0 && fd < MAX_OPEN_FILES) {
        auto& F = fs->files[fd];

        switch(F.type) {
            case FT_Memory:
                ret = F.state.mem.addr == (void*)0xFFFFFFFF;
                break;
            case FT_Null:
                ret = 1;
                break;
            case FT_Zero:
                ret = 0;
                break;
            case FT_Serial:
                ret = 1;
                break;
            case FT_VGA:
                ret = 1;
                break;
            default:
                break;
        }
    }

    return ret;
}

static bool FS_Probe(Volume_Handle handle, void** user) {
    bool ret = false;
    if(handle == 0) {
        DevFS_State* state = (DevFS_State*)kmalloc(sizeof(DevFS_State));
        if(state) {
            ret = true;
            *user = state;
            for(u32 i = 0; i < MAX_OPEN_FILES; i++) {
                state->files[i].type = FT_Invalid;
            }
        }
    }

    return ret;
}

static s32 FS_Sync(void*) {
    return 0;
}

static const char* FS_Name = "Special Device Filesystem";

static Filesystem_Descriptor fsds = {
    .Name = FS_Name,
    .Probe = FS_Probe,
    .Open = FS_Open,
    .Close = FS_Close,
    .Read = FS_Read,
    .Write = FS_Write,
    .Tell = FS_Tell,
    .Seek = FS_Seek,
    .Sync = FS_Sync,
    .EOF = FS_EOF,
};

static Filesystem_Descriptor* Register() {
    return &fsds;
}

REGISTER_FILESYSTEM(Register);