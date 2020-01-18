#include "common.h"
#include "disk.h"
#include "logging.h"
#include "utils.h"
#include "memory.h"
#include "volumes.h"

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
    ASSERT(block_count < 0x7FFFFFFF);

    if(disk < giDisksLastIndex) {
        auto& D = gaDisks[disk];
        ASSERT(D.desc);
        if(D.desc->Write) {
            u32 w;
            if(D.desc->Write(D.user, &w, buf, block_count, block_offset)) {
                ret = (s32)(w & 0x7FFFFFFF);
            }
        } else {
            // Operation is unsupported by the device
        }
    }

    return ret;
}

struct MBR_Entry {
    u8 attr;
    u8 chs_start[3];
    u8 type;
    u8 chs_end[3];
    u32 lbs_start;
    u32 lbs_count;
} PACKED;

void Disk_Partition_Probe() {
    for(int i = 0; i < giDisksLastIndex; i++) {
        u8 mbr[512];
        s32 res = Disk_Read_Blocks(i, mbr, 1, 0);
        if(res == 1) {
            if(mbr[510] == 0x55 && mbr[511] == 0xAA) {
                logprintf("MBR found on disk #%d\n", i);
                for(int j = 0; j < 4; j++) {
                    auto& entry = ((MBR_Entry*)(mbr + 0x1BE))[j];
                    Volume_Descriptor desc;
                    logprintf("- Attr[%x] Type[%x] Start[%x] Count[%x]\n", entry.attr, entry.type, entry.lbs_start, entry.lbs_count);

                    if(entry.lbs_count > 0) {
                        desc.disk = i;
                        desc.offset = entry.lbs_start;
                        desc.length = entry.lbs_count;
                        desc.user = NULL;

                        Volume_Register(&desc);
                    }
                }
            }
        } else {

        }
    }
}