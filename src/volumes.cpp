#include "common.h"
#include "volumes.h"
#include "disk.h"
#include "utils.h"
#include "logging.h"

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
            ret = Disk_Read_Blocks(disk, buffer, count, offset + V.desc.offset);
        } else {
            ret = 0;
        }
    }

    return ret;
}

s32 Volume_Write_Blocks(Volume_Handle vol, const void* buffer, u32 offset, u32 count) {
    s32 ret = -1;

    if(vol < giVolumesLastIndex && buffer) {
        if(count > 0) {
            auto& V = gaVolumes[vol];
            auto disk = V.desc.disk;
            ret = Disk_Write_Blocks(disk, buffer, count, offset + V.desc.offset);
        } else {
            ret = 0;
        }
    }

    return ret;
}

void Volume_Detect_Filesystems() {
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