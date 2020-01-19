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

struct GPT_Partition_Table_Header {
    u64 signature;
    u32 revision;
    u32 header_size;
    u32 crc_header;
    u32 zero;
    u64 lba_this;
    u64 lba_backup;
    u64 lba_data;
    u64 lba_data_end;
    u8 disk_guid[16];
    u64 lba_entries;
    u32 num_entries;
    u32 crc_entries;
    u8 reserved[420];
} PACKED;

struct GPT_Entry {
    u8 part_type[16];
    u8 part_guid[16];
    u64 lba_first;
    u64 lba_last;
    u64 attr;
    u16 name[36];
} PACKED;

#define GPT_ATTR_OEM        (0x0000000000000001)
#define GPT_ATTR_FWIGNORE   (0x0000000000000002)
#define GPT_ATTR_ACTIVE     (0x0000000000000004)
// Basic data partition attrs
#define GPT_ATTR_BDP_RDONLY (0x1000000000000000)
#define GPT_ATTR_BDP_SHADOW (0x2000000000000000)
#define GPT_ATTR_BDP_HIDDEN (0x4000000000000000)
#define GPT_ATTR_BDP_NOLETR (0x8000000000000000)

static bool CanBeGPT(MBR_Entry* entries) {
    bool ret = true;
    u32 cnt = 0;

    for(int i = 0; i < 4; i++) {
        if(entries[i].lbs_count > 0) {
            if(entries[i].type != 0xEE) {
                // Found a partition that is not of type 0xEE
                ret = false;
            } else {
                cnt++;
                if(cnt != 1) {
                    // Found more 0xEE partitions
                    ret = false;
                }
            }
        }
    }

    return ret;
}

static bool IsGPT(u32 disk) {
    bool ret = false;
    u8 lba1[512];
    auto hdr = (GPT_Partition_Table_Header*)lba1;
    s32 rd;

    rd = Disk_Read_Blocks(disk, lba1, 1, 1);
    if(rd == 1) {
        if(hdr->signature == 0x5452415020494645ULL) {
            ret = true;
        }
    }

    return ret;
}

static void ProcessMBR(u32 disk, MBR_Entry* entries) {
    for(int j = 0; j < 4; j++) {
        auto& entry = entries[j];
        Volume_Descriptor desc;
        logprintf("- Attr[%x] Type[%x] Start[%x] Count[%x]\n", entry.attr, entry.type, entry.lbs_start, entry.lbs_count);

        if(entry.lbs_count > 0) {
            desc.disk = disk;
            desc.offset = entry.lbs_start;
            desc.length = entry.lbs_count;

            Volume_Register(&desc);
        }
    }
}

struct GUID {
    u32 time_low;
    u16 time_mid;
    u16 time_hi_ver;
    u16 clock_seq;
    u8 node[6];
} PACKED;

static GUID guid_Unused = {0, 0, 0, 0, {0, 0, 0, 0, 0, 0}};
static GUID guid_BasicData = {0xEBD0A0A2, 0xB9E5, 0x4433, 0xC087, {0x68, 0xB6, 0xB7, 0x26, 0x99, 0xC7}};
//static GUID guid_EFISystem = {0xC12A7328, 0xF81F, 0x11D2, 0x4BBA, {0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B}};

static bool operator==(const GUID& lhs, const GUID& rhs) {
    bool ret = true;
    auto L = (u8*)&lhs; auto R = (u8*)&rhs;
    for(int i = 0; i < 16; i++) ret &= (L[i] == R[i]);
    return ret;
}
static bool operator!=(const GUID& lhs, const GUID& rhs) { return !(lhs == rhs); }

static void PrintGUID(u8* guid) {
    GUID* g = (GUID*)guid;
    logprintf("%x-%x-%x-%x-", g->time_low, g->time_mid, g->time_hi_ver, g->clock_seq);
    for(int i = 0; i < 6; i++) {
        char c[3] = {0, 0, 0};
        u32 xl = (g->node[i] & 0x0F) >> 0;
        u32 xh = (g->node[i] & 0xF0) >> 4;
        c[0] = xh < 10 ? '0' + xh : 'A' + xh - 10;
        c[1] = xl < 10 ? '0' + xl : 'A' + xl - 10;
        logprintf("%s", c);
    }
}

static void ProcessGPT(u32 disk) {
    Volume_Descriptor desc;
    u8 buf[512];
    GPT_Partition_Table_Header hdr;
    auto phdr = (GPT_Partition_Table_Header*)buf;

    s32 rd;

    rd = Disk_Read_Blocks(disk, buf, 1, 1);
    ASSERT(rd == 1);

    hdr = *phdr;
    phdr = NULL;

    u32 lba_cur = hdr.lba_entries;
    for(u32 i = 0; i < hdr.num_entries; i++) {
        u32 bufidx = i % 4;
        if(bufidx == 0) {
            ASSERT(lba_cur < hdr.lba_data);
            rd = Disk_Read_Blocks(disk, buf, 1, lba_cur);
            lba_cur += 1;
        }
        GPT_Entry& entry = ((GPT_Entry*)buf)[bufidx];
        u8 name[37];
        for(int i = 0; i < 36; i++) {
            name[i] = (entry.name[i] & 0xFF);
        }
        name[36] = 0;

        auto& type = *(GUID*)entry.part_type;
        if(type != guid_Unused) {
            logprintf("Partition %x off=%x name='%s' type=", i, entry.lba_first, name);
            PrintGUID(entry.part_type);
            logprintf("\n");

            if((entry.attr & GPT_ATTR_OEM) == 0 && type == guid_BasicData) {
                desc.disk = disk;
                desc.offset = entry.lba_first;
                desc.length = entry.lba_last - entry.lba_first + 1;
                Volume_Register(&desc);
            }
        }
    }
}

void Disk_Partition_Probe() {
    for(u32 i = 0; i < giDisksLastIndex; i++) {
        u8 mbr[512];
        s32 res = Disk_Read_Blocks(i, mbr, 1, 0);
        if(res == 1) {
            if(mbr[510] == 0x55 && mbr[511] == 0xAA) {
                logprintf("MBR found on disk #%d\n", i);
                auto entries = (MBR_Entry*)(mbr + 0x1BE);
                if(CanBeGPT(entries)) {
                    if(IsGPT(i)) {
                        ProcessGPT(i);
                    }
                } else {
                    ProcessMBR(i, entries);
                }
            }
        } else {

        }
    }
}