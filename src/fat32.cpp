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

#define FAT32_MAX_OPEN_FILES (64)

struct FAT32_File {
    bool valid;
    u32 cluster_start;
    u32 current_cluster;
    u32 offset;
};

struct FAT32_State {
    Volume_Handle vol;
    u32 sectors_per_cluster; // sectors per cluster
    u32 sector_fat0; // offset to first FAT
    u32 sector_info; // offset to info sector
    u32 sector_count; // sector count
    u32 cluster_root_dir; // cluster of root directory
    u32 sector_offset_data_region; // First sector of the data region

    u32 cluster_cache_index; // (0xFFFFFFFF -> cache is invalid)
    bool cluster_cache_dirty;
    u8* cluster_cache; // size=sectors_per_cluster*512

    u32 fat_cache_index; //
    bool fat_cache_dirty;
    u8 fat_cache[512]; // Cache of one sector from the FAT

    u32 free_file_handles;
    FAT32_File files[FAT32_MAX_OPEN_FILES];
};

enum Dirent_Attr {
    DEA_ReadOnly = 0x01,
    DEA_Hidden = 0x02,
    DEA_System = 0x04,
    DEA_VolumeLabel = 0x08,
    DEA_Subdirectory = 0x10,
    DEA_Archive = 0x20,
    DEA_Device = 0x40,
    DEA_Reserved = 0x80,
};

struct Dirent {
    u8 filename[8];
    u8 extension[3];
    u8 attr;
    u8 vfat_attr; // zero until we support vfat
    u8 unk0;
    u16 ctime_time;
    u16 ctime_date;
    u16 atime_date;
    u16 cluster_hi;
    u16 mtime_time;
    u16 mtime_date;
    u16 cluster_lo;
    u32 size;
} PACKED;

// -----------------------------------------------------------------------------------------

// Maps a cluster index to the index of the sector where the cluster's entry
// is located in the FAT
static inline u32 ClusterIndexToFATSectorIndex(FAT32_State* fs, u32 cluster_idx) {
    // One sector contains 128 doublewords
    return cluster_idx / 128;
}

static u8* LoadCluster(FAT32_State* fs, u32 cluster_idx) {
    ASSERT(fs);
    u8* ret = fs->cluster_cache;
    u32 sector_offset;

    // TODO: range check sector_idx

    if(fs->cluster_cache_index != cluster_idx) {
        if(fs->cluster_cache_dirty) {
            // Write cached sector back
            sector_offset = fs->sector_offset_data_region + (fs->cluster_cache_index - 2) * fs->sectors_per_cluster;
            logprintf("FAT32: evicting cluster #%d (sector=%x) from cache\n", fs->cluster_cache_index, sector_offset);
            Volume_Write_Blocks(fs->vol, fs->cluster_cache, sector_offset, fs->sectors_per_cluster);
            fs->cluster_cache_dirty = false;
        }
        // Load new cluster
        sector_offset = fs->sector_offset_data_region + (cluster_idx - 2) * fs->sectors_per_cluster;
        logprintf("Cluster Idx: %x Sectors per cluster: %d Data offset: %x -> Sector offset: %x\n", cluster_idx, fs->sectors_per_cluster, fs->sector_offset_data_region, sector_offset);
        logprintf("FAT32: loading cluster #%d (sector=%x) into cache\n", cluster_idx, sector_offset);
        Volume_Read_Blocks(fs->vol, fs->cluster_cache, sector_offset, fs->sectors_per_cluster);
    }

    return ret;
}

static inline char ToUpper(char ch) {
    if(ch >= 'a' && ch <= 'z') {
        return (ch - 32);
    } else {
        return ch;
    }
}

static void LoadFATPage(FAT32_State* fs, u32 page) {
    u32 sector_offset;
    if(fs->fat_cache_dirty) {
        sector_offset = fs->sector_fat0 + fs->fat_cache_index;
        logprintf("FAT32: evicting FAT page #%d (sector=%x) from cache\n", fs->fat_cache_index, sector_offset);
        Volume_Write_Blocks(fs->vol, fs->fat_cache, sector_offset, 1);
    }
    
    // Load new page
    sector_offset = fs->sector_fat0 + page;
    logprintf("FAT32: loading FAT page #%d (sector=%x) into cache\n", page, sector_offset);
    Volume_Read_Blocks(fs->vol, fs->fat_cache, sector_offset, 1);
}

static u32 GetFATEntry(FAT32_State* fs, u32 cluster_idx) {
    ASSERT(fs);
    auto fat_page = ClusterIndexToFATSectorIndex(fs, cluster_idx);
    if(fs->fat_cache_index != fat_page) {
        LoadFATPage(fs, fat_page);
    }
    // auto off = (cluster_idx) % 128;
    auto off = cluster_idx & 127;
    return ((u32*)fs->fat_cache)[off];
}

static u32 FindClusterOfEntryInDirectory(FAT32_State* fs, u32 cluster_dir, bool* is_subdirectory, bool* is_readonly, const char* path, u32 path_len) {
    u32 ret = 0;
    ASSERT(fs);
    ASSERT(path);
    bool found = false;

    const auto entries_per_cluster = (fs->sectors_per_cluster * 512) / 32;
    // TODO: range check cluster_dir
    while(!found && (cluster_dir & 0xFFFFFFF0) != 0x0FFFFFF0) {
        auto dir = LoadCluster(fs, cluster_dir);

        Dirent* entries = (Dirent*)dir;
        for(u32 i = 0; i < entries_per_cluster && !found; i++) {
            auto& ent = entries[i];
            u8 buf[16];
            int c = 7;

            if(ent.filename[0] == 0x00) {
                // End-of-directory
                continue;
            }

            if((ent.attr & DEA_VolumeLabel) || (ent.attr & DEA_Device)) {
                continue;
            }

            while(ent.filename[c] == ' ' && c >= 0) {
                c--;
            }

            auto fnl = c + 1;
            //for(; c >= 0; c--) {
            for(int i = 0; i < fnl; i++) {
                buf[i] = ent.filename[i];
            }
            buf[fnl] = '.';
            buf[fnl + 1] = ent.extension[0];
            buf[fnl + 2] = ent.extension[1];
            buf[fnl + 3] = ent.extension[2];
            buf[fnl + 4] = 0;

            if(buf[fnl + 3] == ' ') {
                buf[fnl + 3] = 0;
                if(buf[fnl + 2] == ' ') {
                    buf[fnl + 2] = 0;
                    if(buf[fnl + 1] == ' ') {
                        buf[fnl + 1] = 0;
                        buf[fnl + 0] = 0;
                    }
                }
            }

            logprintf("%% %s\n", buf);

            // Calc filename length
            int len;
            for(len = 0; buf[len]; len++);
            
            if(len == path_len) {
                found = true;
                int i = 0;

                for(int i = 0; i < len; i++) {
                    if(buf[i] != path[i]) {
                        found = false;
                    }
                }

                if(found) {
                    ret = (((u32)ent.cluster_hi) << 16) | (u32)ent.cluster_lo;

                    if(is_subdirectory != NULL) {
                        *is_subdirectory = (ent.attr & DEA_Subdirectory) != 0;
                    }
                    if(is_readonly != NULL) {
                        *is_readonly = (ent.attr & DEA_ReadOnly) != 0;
                    }
                }
            }
        }

        // Get next cluster
        cluster_dir = GetFATEntry(fs, cluster_dir);
    }

    return ret;
}

// -----------------------------------------------------------------------------------------

static Filesystem_File_Handle FS_Open(void* user, const char* path, mode_t flags) {
    auto state = (FAT32_State*)user;
    Filesystem_File_Handle ret = -1;

    logprintf("FS_Open: '%s'\n", path);

    bool path_end = false;
    u32 slash_idx = 0;
    u32 current_directory_cluster = state->cluster_root_dir;
    auto P = path + 1; // skip initial slash

    bool found = false;
    u32 start_cluster = 0;

    if(state->free_file_handles > 0) {
        while(1) {
            // Find next slash index
            while(P[slash_idx] != '/' && P[slash_idx] != '\0') {
                slash_idx++;
            }

            //logprintf("Fragment: '%s' L=%d\n", P, slash_idx);
            bool is_subdirectory = false, is_readonly = false;

            u32 next_cluster = FindClusterOfEntryInDirectory(state, current_directory_cluster, &is_subdirectory, &is_readonly, P, slash_idx);

            if(next_cluster != 0) {
                bool end_of_path = P[slash_idx] == '\0';
                if(end_of_path) {
                    if(!is_subdirectory) {
                        start_cluster = next_cluster;
                        found = true;
                    }
                    break;
                }

                current_directory_cluster = next_cluster;

                P += slash_idx + 1;
                slash_idx = 0;
            } else {
                // ENOENT
                break;
            }
        }

        if(found) {
            int fd;
            for(fd = 0; fd < FAT32_MAX_OPEN_FILES; fd++) {
                if(!state->files[fd].valid) {
                    auto& f = state->files[fd];
                    f.cluster_start = start_cluster;
                    f.current_cluster = start_cluster;
                    f.offset = 0;
                    f.valid = true;
                    ret = (Filesystem_File_Handle)fd;
                    break;
                }
            }

            state->free_file_handles -= 1;
        } else {
            logprintf("'%d:%s': file not found\n", state->vol, path);
        }
    } else {
        logprintf("'%d:%s': can't open file: out of file handles\n", state->vol, path);
    }

    return ret;
}

static void FS_Close(void* user, Filesystem_File_Handle fd) {
    auto state = (FAT32_State*)user;
    ASSERT(state);

    if(fd < FAT32_MAX_OPEN_FILES) {
        if(state->files[fd].valid) {
            // TODO: flush cache when it's eventually implemented
            state->files[fd].valid = false;
            state->free_file_handles += 1;

            ASSERT(state->free_file_handles <= FAT32_MAX_OPEN_FILES);
        }
    }
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

static s32 FS_Seek(void* user, Filesystem_File_Handle fd, whence_t whence, s32 position) {
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

                    auto sect_per_fat = (bootsect->sectors_per_fat == 0) ? bootsect->sectors_per_fat32 : bootsect->sectors_per_fat;
                    state->sector_offset_data_region = bootsect->count_reserved + bootsect->count_fat * sect_per_fat;

                    for(int fd = 0; fd < FAT32_MAX_OPEN_FILES; fd++) {
                        state->files[fd].valid = false;
                    }
                    state->free_file_handles = FAT32_MAX_OPEN_FILES;

                    *user = state;
                    // Put first sector of FAT into cache
                    memcpy(state->fat_cache, buf_fat, 512);
                    state->fat_cache_index = 0;

                    // Allocate cluster cache
                    state->cluster_cache = (u8*)kmalloc(state->sectors_per_cluster * 512);
                    // Put cluster 0 into cache
                    LoadCluster(state, 0);
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