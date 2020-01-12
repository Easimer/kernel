#include "common.h"
#include "disk.h"
#include "logging.h"
#include "utils.h"
#include "memory.h"

struct Disk_Device {
    void* user;
    const Disk_Device_Descriptor* desc;
};

#define MAX_DISKS (64)

static Disk_Device gaDisks[MAX_DISKS];
static u32 giDisksLastIndex = 0;

bool Disk_Register_Device(void* user, const Disk_Device_Descriptor* desc) {
    bool ret = false;

    if(desc) {
        if(giDisksLastIndex < MAX_DISKS) {
            gaDisks[giDisksLastIndex].user = user;
            gaDisks[giDisksLastIndex].desc = desc;

            logprintf("Registered disk #%d (%x, %x)\n", giDisksLastIndex, user, desc);

            giDisksLastIndex++;
            ret = true;
        }
    }

    return ret;
}

bool Disk_Exists(u32 disk) {
    return disk < giDisksLastIndex;
}

u32 Disk_BlockSize(u32 disk) {
    u32 ret = 0;

    if(disk < giDisksLastIndex) {
        ASSERT(gaDisks[disk].desc);
        ret = gaDisks[disk].desc->BlockSize;
    }

    return ret;
}

s32 Disk_Read_Blocks(u32 disk, void* buf, u32 block_count, u32 block_offset) {
    s32 ret = -1;
    ASSERT(block_count < 0x7FFFFFFF);

    if(disk < giDisksLastIndex) {
        auto& D = gaDisks[disk];
        ASSERT(D.desc);
        if(D.desc->Read) {
            u32 r;
            if(D.desc->Read(D.user, &r, buf, block_count, block_offset)) {
                ret = (s32)(r & 0x7FFFFFFF);
            }
        } else {
            // Operation is unsupported by the device
        }
    }

    return ret;
}

s32 Disk_Write_Blocks(u32 disk, const void* buf, u32 block_count, u32 block_offset) {
    s32 ret = -1;

    return ret;
}