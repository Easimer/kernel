#include "common.h"
#include "volumes.h"
#include "fat32.h"
#include "utils.h"
#include "logging.h"
#include "memory.h"

struct FAT32_Boot_Sector {
    u8 jump[3];
    u8 oem[8];
    // BPB Common
    u16 sector_size; // Logical cluster size
    u8 sectors_per_cluster; // 2**[0 .. 7]
    u16 count_reserved; // Count of reserved sectors
    u8 count_fat; // Count of FATs
    u16 max_root_dent; // Maximum number of root directory entries (0 for FAT32)
    u16 total_sectors; // Total number of logical sectors (use total_sectors32 if this value is zero)
    u8 media;
    u16 sectors_per_fat; // unused (0x24)
    // BPB 3.31
    u16 sectors_per_track;
    u16 heads;
    u32 count_of_hidden_sectors; // C.o.H.S. that preceed this partition
    u32 total_sectors32; // use this if total_sectors is zero
    // BPB F32 Ext
    u32 sectors_per_fat32;
    u16 mirror_flags; // if bit 7 is 1, bits 0-3 indicate the ID of the active FAT
    u16 version; // zero
    u32 cluster_root_directory;
    u16 sector_infosector;
    u16 sector_backup;
    u8 reserved[12];
} PACKED;

struct FAT32_Info_Sector {
    u32 signature; // RRaA
    u8 reserved[480];
    u32 sector_signature; // rrAa
    u32 free_data_clusters;
    u32 most_recent_known_allocated_cluster;
    u8 reserved2[12];
    u8 signature_tail[4]; // 0x00 0x00 0x55 0xAA
} PACKED;

struct FAT32_State {
    Volume_Handle vol;
    u32 sectors_per_cluster; // sectors per cluster
    u32 sector_fat0; // offset to first FAT
    u32 sector_info; // offset to info sector
    u32 sector_count; // sector count
    u32 cluster_root_dir; // cluster of root directory

    u32 sector_cache_index; // (0xFFFFFFFF -> cache is invalid)
    u8 sector_cache[512];
};

// -----------------------------------------------------------------------------------------



// -----------------------------------------------------------------------------------------

static Filesystem_File_Handle FS_Open(void* user, const char* path, mode_t flags) {
    auto state = (FAT32_State*)user;
}

static void FS_Close(void* user, Filesystem_File_Handle fd) {
    auto state = (FAT32_State*)user;
}

static s32 FS_Read(void* user, Filesystem_File_Handle fd, void* dst, s32 bytes) {
    auto state = (FAT32_State*)user;
}

static s32 FS_Write(void* user, Filesystem_File_Handle fd, const void* src, s32 bytes) {
    auto state = (FAT32_State*)user;
}

static s32 FS_Tell(void* user, Filesystem_File_Handle fd) {
    auto state = (FAT32_State*)user;
}

static s32 FS_Seek(void* user, Filesystem_File_Handle fd, whence_t whence, u32 position) {
    auto state = (FAT32_State*)user;
}

static s32 FS_Sync(void* user) {
    auto state = (FAT32_State*)user;
}

static bool FS_Probe(Volume_Handle handle, void** user) {
    bool ret = false;
    u8 buf_bootsect[512];
    u8 buf_infosect[512];
    u8 buf_fat[512];
    auto bootsect = (FAT32_Boot_Sector*)buf_bootsect;
    auto infosect = (FAT32_Info_Sector*)buf_infosect;

    //logprintf("Testing volume #%d for FAT32 presence\n", handle);

    if(Volume_Read_Blocks(handle, buf_bootsect, 0, 1)) {
        u8 buf_oem[9];
        memcpy(buf_oem, bootsect->oem, 8);
        buf_oem[8] = 0;
        //logprintf("\tOEM: %s\n", buf_oem);
        //logprintf("\tSector size: %d\n\tSectors per cluster: %d\n\tCount of FATs: %d\n\tTotal sectors: %d (%d)\n\tSectors per FAT: %d\n\tInfosector: %x\n",
            //bootsect->sector_size, bootsect->sectors_per_cluster, bootsect->count_fat, bootsect->total_sectors, bootsect->total_sectors32, bootsect->sectors_per_fat32, bootsect->sector_infosector);
        
        if(Volume_Read_Blocks(handle, buf_infosect, bootsect->sector_infosector, 1)) {
            //logprintf("\tInfosector is present:\n");
            //logprintf("\t\tSignature: %x\n\t\tSignature 2: %x\n\t\tFree clusters: %x\n",
            //infosect->signature, infosect->sector_signature, infosect->free_data_clusters);

            // Bootsect signature is okay
            bool bs_signature = (buf_bootsect[510] == 0x55) && (buf_bootsect[511] == 0xAA);
            // Infosect signatures are okay
            bool is_signature = infosect->signature == 0x41615252 && infosect->sector_signature == 0x61417272;
            bool version_ok = bootsect->version == 0x0000;

            if(Volume_Read_Blocks(handle, buf_fat, bootsect->count_reserved, 1)) {
                u32* fat = (u32*)buf_fat;
                u32 cluster0 = fat[0];
                u32 cluster0eoc = fat[1];

                auto fat_id = bootsect->media;
                u32 cluster0_expected = 0x0FFFFF00 | fat_id;

                bool cluster0_ok = (cluster0 == cluster0_expected) && (cluster0eoc == 0x0FFFFFFF);

                ret = bs_signature && is_signature && version_ok && cluster0_ok;

                if(ret) {
                    auto state = (FAT32_State*)kmalloc(sizeof(FAT32_State));
                    ASSERT(state);
                    state->vol = handle;
                    state->sectors_per_cluster = bootsect->sectors_per_cluster;
                    state->sector_fat0 = bootsect->count_reserved;
                    state->sector_info = bootsect->sector_infosector;
                    state->sector_count = bootsect->total_sectors == 0 ? bootsect->total_sectors32 : bootsect->total_sectors;
                    state->cluster_root_dir = bootsect->cluster_root_directory;
                    state->sector_cache_index = 0xFFFFFFFF;

                    *user = state;

                    // Precache first sector of FAT
                    memcpy(state->sector_cache, buf_fat, 512);
                    state->sector_cache_index = bootsect->count_reserved;
                }
            }
        } else {
            logprintf("\tFailed to read the information sector\n");
        }
    } else {
        logprintf("\tFailed to read sector 0\n");
    }

    return ret;
}

static const char* FS_Name = "FAT32";

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
};

static Filesystem_Descriptor* Register() {
    return &fsds;
}

REGISTER_FILESYSTEM(Register);