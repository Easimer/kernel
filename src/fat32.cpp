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

template<typename T>
struct Cluster_Index {
public:
    constexpr Cluster_Index() : value(0) {}
    explicit constexpr Cluster_Index(u32 val) : value(val) {}
    constexpr u32 operator=(u32 val) { value = val; return value;}
    template<typename U> void operator=(const Cluster_Index<U>&) = delete;
    constexpr operator u32() const { return value; }
    constexpr operator u32&() { return value; }

private:
    u32 value;
};

// The first cluster after the FAT is actually Cluster #2
// Translated cluster index (VCI(2) -> First data cluster, Third FAT entry)
using Virtual_Cluster_Index = Cluster_Index<struct Tag_Virtual>;
// Physical cluster index (RCI(0) -> VCI(2))
using Physical_Cluster_Index = Cluster_Index<struct Tag_Physical>;

constexpr inline Virtual_Cluster_Index clsptov(Physical_Cluster_Index pci) {
    return Virtual_Cluster_Index(pci + (u32)2);
}

constexpr inline Physical_Cluster_Index clsvtop(Virtual_Cluster_Index vci) {
    return Physical_Cluster_Index(vci - (u32)2);
}

struct FAT32_File {
    bool valid;
    Virtual_Cluster_Index cluster_start;
    Virtual_Cluster_Index current_cluster;
    u32 offset;
    u32 size;
    Virtual_Cluster_Index cluster_dirent; // the cluster where this file's dirent is stored
};

struct FAT32_State {
    Volume_Handle vol;
    u32 sectors_per_cluster; // sectors per cluster
    u32 sector_fat0; // offset to first FAT
    u32 sector_info; // offset to info sector
    u32 sector_count; // sector count
    Virtual_Cluster_Index cluster_root_dir; // cluster of root directory
    u32 sector_offset_data_region; // First sector of the data region
    u32 cluster_size; // cluster size in bytes
    u32 sectors_per_fat; // number of sectors in a FAT
    bool write_protected; // is write protected

    Virtual_Cluster_Index cluster_cache_index; // (0xFFFFFFFF -> cache is invalid)
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

#define CLUSTER_HI(idx) (((u32)(idx)) >> 16)
#define CLUSTER_LO(idx) (((u32)(idx)) & 0xFFFF)
// End-of-chain marker
#define CLUSTER_EOC (0x0FFFFFFF)

#define MARK_CLUSTER_CACHE_DIRTY(fs) fs->cluster_cache_dirty = true

#define ClusterIndexWords(vci, hi, lo) hi = CLUSTER_HI(vci); lo = CLUSTER_LO(vci);

// -----------------------------------------------------------------------------------------

// Maps a cluster index to the index of the sector where the cluster's entry
// is located in the FAT
static inline u32 ClusterIndexToFATPageIndex(FAT32_State* fs, Virtual_Cluster_Index cluster_idx) {
    (void)fs;
    // One sector contains 128 doublewords
    return (u32)cluster_idx / 128;
}

#define ClusterIndexToFATSectorIndex ClusterIndexToFATPageIndex

static void FlushFATPage(FAT32_State* fs) {
    u32 sector_offset = fs->sector_fat0 + fs->fat_cache_index;
    //logprintf("FAT32: flushing FAT page #%d (sector=%x) from cache\n", fs->fat_cache_index, sector_offset);
    Volume_Write_Blocks(fs->vol, fs->fat_cache, sector_offset, 1);
    fs->fat_cache_dirty = false;
}

static void FlushClusterCache(FAT32_State* fs) {
    ASSERT(fs);
    u32 sector_offset;
    if(fs->cluster_cache_dirty) {
        // Write cached sector back
        //sector_offset = fs->sector_offset_data_region + (fs->cluster_cache_index - 2) * fs->sectors_per_cluster;
        sector_offset = fs->sector_offset_data_region + clsvtop(fs->cluster_cache_index) * fs->sectors_per_cluster;
        //logprintf("FAT32: evicting cluster #%d (sector=%x) from cache\n", fs->cluster_cache_index, sector_offset);
        Volume_Write_Blocks(fs->vol, fs->cluster_cache, sector_offset, fs->sectors_per_cluster);
        fs->cluster_cache_dirty = false;
        //logprintf("FAT32: cluster %x written back!\n", fs->cluster_cache_index);
    }
}

static u8* LoadCluster(FAT32_State* fs, Virtual_Cluster_Index cluster_idx) {
    ASSERT(fs);
    u8* ret = fs->cluster_cache;
    u32 sector_offset;

    // TODO: range check sector_idx

    if(fs->cluster_cache_index != cluster_idx) {
        FlushClusterCache(fs);
        // Load new cluster
        //sector_offset = fs->sector_offset_data_region + (cluster_idx - 2) * fs->sectors_per_cluster;
        sector_offset = fs->sector_offset_data_region + clsvtop(cluster_idx) * fs->sectors_per_cluster;
        logprintf("Cluster Idx: %x Sectors per cluster: %d Data offset: %x -> Sector offset: %x\n", cluster_idx, fs->sectors_per_cluster, fs->sector_offset_data_region, sector_offset);
        logprintf("FAT32: loading cluster #%d (sector=%x) into cache\n", cluster_idx, sector_offset);
        Volume_Read_Blocks(fs->vol, fs->cluster_cache, sector_offset, fs->sectors_per_cluster);
        fs->cluster_cache_index = Virtual_Cluster_Index(cluster_idx);
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
    ASSERT(fs);
    
    ASSERT(page < fs->sectors_per_fat);

    if(fs->fat_cache_dirty) {
        FlushFATPage(fs);
    }
    
    // Load new page
    sector_offset = fs->sector_fat0 + page;
    //logprintf("FAT32: loading FAT page #%d (sector=%x) into cache\n", page, sector_offset);
    Volume_Read_Blocks(fs->vol, fs->fat_cache, sector_offset, 1);
    fs->fat_cache_index = page;
}

static u32 GetFATEntry(FAT32_State* fs, Virtual_Cluster_Index cluster_idx) {
    ASSERT(fs);
    auto fat_page = ClusterIndexToFATSectorIndex(fs, cluster_idx);
    if(fs->fat_cache_index != fat_page) {
        LoadFATPage(fs, fat_page);
    }
    // auto off = (cluster_idx) % 128;
    auto off = cluster_idx & 127;
    return ((u32*)fs->fat_cache)[off];
}

static void SetFATEntry(FAT32_State* fs, Virtual_Cluster_Index cluster_idx, u32 value, bool flush = false) {
    ASSERT(fs);
    //logprintf("Setting FAT entry %x to %x\n", cluster_idx, value);
    auto fat_page = ClusterIndexToFATSectorIndex(fs, cluster_idx);
    if(fs->fat_cache_index != fat_page) {
        LoadFATPage(fs, fat_page);
    }
    auto off = cluster_idx & 127;
    //logprintf("\tPage=%x Off=%x\n", fat_page, off);
    ((u32*)fs->fat_cache)[off] = value;
    fs->fat_cache_dirty = true;

    if(flush) {
        FlushFATPage(fs);
    }
}

// Returns 0 on failure
static Virtual_Cluster_Index AllocateCluster(FAT32_State* fs) {
    ASSERT(fs);
    Virtual_Cluster_Index ret;

    for(u32 i = 0; i < fs->sectors_per_fat && ret == 0; i++) {
        LoadFATPage(fs, i);
        u32* fat = (u32*)fs->fat_cache;
        for(u32 cls = 0; cls < 128 && ret == 0; cls++) {
            if(fat[cls] == 0) {
                fat[cls] = CLUSTER_EOC;
                fs->fat_cache_dirty = true;
                ret = i * 128 + cls;
                //logprintf("FAT32: Alloc page=%x idx=%x cls=%x\n", i, cls, ret);

                // Zero-fill the newly allocated cluster
                
                // Save cached cluster index
                auto old_cached_cluster = fs->cluster_cache_index;
                LoadCluster(fs, ret);
                memset(fs->cluster_cache, 0, fs->cluster_size);
                MARK_CLUSTER_CACHE_DIRTY(fs);
                // Restore old cluster
                LoadCluster(fs, old_cached_cluster);
            }
        }
    }

    if(ret == 0) {
        logprintf("FAT32: volume %x is out of space\n", fs->vol);
    }

    return ret;
}

static void PutFilenameIntoDirent(Dirent& dent, const char* filename) {
    int i = 0, dot, j;

    // TODO: prevent filename collisions

    for(i = 0; i < 8 && filename[i] != '.'; i++) {
        dent.filename[i] = filename[i];
    }
    if(i < 8 && filename[i] == '.') {
        // Pad with spaces
        dot = i;
        while(i < 8) {
            dent.filename[i] = ' ';
            i++;
        }
    } else {
        while(filename[i] != '.' && filename[i] != '\0') {
            i++;
        }
        dot = i;
    }
    if(filename[dot] != '\0') {
        i = dot + 1;
        j = 0;
        while(filename[i] != '\0' && j < 3) {
            dent.extension[j] = filename[i];
            i++;
            j++;
        }
    }
}

static Virtual_Cluster_Index NextCluster(FAT32_State* fs, Virtual_Cluster_Index cluster_idx) {
    ASSERT(fs);
    Virtual_Cluster_Index ret;

    u32 entry = GetFATEntry(fs, cluster_idx);

    //logprintf("Next in chain cluster of %x is %x\n", cluster_idx, entry);

    if(entry == CLUSTER_EOC) {
        ret = AllocateCluster(fs);
        //logprintf("\tAllocated new cluster %x\n", ret);
        if(ret != 0) {
            SetFATEntry(fs, cluster_idx, ret);
            SetFATEntry(fs, ret, CLUSTER_EOC);
        }
    } else {
        ret = entry;
    }

    return ret;
}

// Given a directory cluster ID it inserts the given Dirent into the directory.
// Returns true on success, false when the FS is out of space.
static bool InsertIntoDirectory(FAT32_State* fs, Virtual_Cluster_Index cluster_directory, const Dirent& ent) {
    bool ret = false, out_of_space = false;
    ASSERT(fs);
    Dirent* entries;
    const auto count_ents = fs->cluster_size / sizeof(Dirent);

    // Look for a free entry
    // If we didn't found a free entry: nextcluster
    Virtual_Cluster_Index current = cluster_directory;
    entries = (Dirent*)LoadCluster(fs, current);

    while(!ret && !out_of_space) {
        for(u32 idx = 0; idx < count_ents && !ret; idx++) {
            if(entries[idx].filename[0] == 0x00) {
                memcpy(entries + idx, &ent, sizeof(Dirent));
                MARK_CLUSTER_CACHE_DIRTY(fs);
                ret = true;
            }
        }

        if(!ret) {
            current = NextCluster(fs, current);
            // TODO: somehow we should make sure we allocate at most one new cluster
            if(current == 0) {
                out_of_space = true;
            }
        }
    }

    return ret;
}

// Returns the sector index of the directory entry
static Virtual_Cluster_Index FindClusterOfEntryInDirectory(FAT32_State* fs, Virtual_Cluster_Index cluster_dir, Dirent* dent, const char* path, u32 path_len) {
    Virtual_Cluster_Index ret;
    ASSERT(fs);
    ASSERT(path);
    bool found = false;
    u32 counter = 0;

    const auto entries_per_cluster = (fs->sectors_per_cluster * 512) / 32;
    // TODO: range check cluster_dir
    while(!found && (cluster_dir & 0xFFFFFFF0) != 0x0FFFFFF0) {
        logprintf("Loading cluster %x\n", cluster_dir);
        auto dir = LoadCluster(fs, cluster_dir);

        Dirent* entries = (Dirent*)dir;
        for(u32 i = 0; i < entries_per_cluster && !found; i++) {
            auto& ent = entries[i];
            u8 buf[16];
            int c = 7;
            counter++;

            if(counter == 65536) {
                // Directory is too big
                // TODO: this is a workaround
                ret = 0;
                return ret;
            }

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

            //logprintf("%% %s\n", buf);

            // Calc filename length
            u32 len;
            for(len = 0; buf[len]; len++);
            
            if(len == path_len) {
                found = true;

                for(u32 i = 0; i < len; i++) {
                    if(buf[i] != path[i]) {
                        found = false;
                    }
                }

                if(found) {
                    //ret = (((u32)ent.cluster_hi) << 16) | (u32)ent.cluster_lo;
                    ret = cluster_dir;

                    if(dent != NULL) {
                        memcpy(dent, &ent, sizeof(ent));
                    }
                }
            }
        }

        // Get next cluster
        cluster_dir = GetFATEntry(fs, cluster_dir);
    }
    logprintf("End of directory\n");

    return ret;
}

// -----------------------------------------------------------------------------------------

static Filesystem_File_Handle FS_Open(void* user, const char* path, mode_t flags) {
    auto state = (FAT32_State*)user;
    ASSERT(state);
    Filesystem_File_Handle ret = -1;

    u32 slash_idx = 0;
    auto current_directory_cluster = state->cluster_root_dir;
    auto P = path + 1; // skip initial slash

    bool found = false;
    Virtual_Cluster_Index start_cluster;
    Dirent dent;
    Virtual_Cluster_Index dirent_cluster;

    if(state->write_protected && (flags & (O_WRONLY | O_CREAT))) {
        //logprintf("Won't open file for writing: write protected\n");
        return -1;
    }

    if(state->free_file_handles > 0) {
        while(1) {
            // Find next slash index
            while(P[slash_idx] != '/' && P[slash_idx] != '\0') {
                slash_idx++;
            }

            //logprintf("Fragment: '%s' L=%d\n", P, slash_idx);
            bool is_subdirectory = false;

            dirent_cluster = FindClusterOfEntryInDirectory(state, current_directory_cluster, &dent, P, slash_idx);

            is_subdirectory = (dent.attr & DEA_Subdirectory) != 0;
            bool end_of_path = P[slash_idx] == '\0';

            logprintf("dirent_cluster=%x\n", dirent_cluster);

            if(dirent_cluster != 0) {
                u32 dent_cluster = (((u32)dent.cluster_hi) << 16) | (u32)dent.cluster_lo;
                if(end_of_path) {
                    if(!is_subdirectory) {
                        start_cluster = dent_cluster;
                        found = true;
                    }
                    break;
                }

                current_directory_cluster = dent_cluster;

                P += slash_idx + 1;
                slash_idx = 0;
            } else {
                if(flags & O_CREAT) {
                    if(current_directory_cluster != 0 && end_of_path) {
                        Dirent ent;
                        memset(&ent, 0, sizeof(Dirent));
                        start_cluster = AllocateCluster(state);
                        ClusterIndexWords(start_cluster, ent.cluster_hi, ent.cluster_lo);
                        PutFilenameIntoDirent(ent, P);
                        dirent_cluster = current_directory_cluster;
                        dent = ent;
                        if(InsertIntoDirectory(state, current_directory_cluster, ent)) {
                            found = true;
                        } else {
                            //logprintf("FAT32: couldn't create file: OUT OF SPACE\n");
                        }
                        break;
                    } else {
                        //logprintf("FAT32: Would create file, but path doesn't exist!\n");
                        break;
                    }
                } else {
                    //logprintf("FAT32: not found and not creating\n");
                    break;
                }
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
                    f.size = dent.size;
                    f.cluster_dirent = dirent_cluster;
                    ret = (Filesystem_File_Handle)fd;
                    break;
                }
            }

            state->free_file_handles -= 1;
            logprintf("found file\n");
        } else {
            logprintf("'%d:%s': file not found\n", state->vol, path);
        }
    } else {
        //logprintf("'%d:%s': can't open file: out of file handles\n", state->vol, path);
    }

    return ret;
}

static void FS_Close(void* user, Filesystem_File_Handle fd) {
    auto state = (FAT32_State*)user;
    ASSERT(state);

    if(fd < FAT32_MAX_OPEN_FILES) {
        if(state->files[fd].valid) {
            auto& F = state->files[fd];
            // TODO: write back directory entry
            // (1) iterate over dirents
            //logprintf("FAT32: Updating back dirent\n");
            auto entries = (Dirent*)LoadCluster(state, F.cluster_dirent);
            bool found = false;
            auto entries_per_cluster = state->cluster_size / sizeof(Dirent);
            u32 cluster_hi = CLUSTER_HI(F.cluster_start);
            u32 cluster_lo = CLUSTER_LO(F.cluster_start);
            for(u32 i = 0; i < entries_per_cluster && !found; i++) {
                auto& ent = entries[i];

                if(ent.filename[0] == 0x00) {
                    // Empty entry
                    continue;
                }

                if(ent.cluster_hi == cluster_hi && ent.cluster_lo == cluster_lo) {
                    ent.size = F.size;
                    //logprintf("New size is %x, start cluster=%x\n", F.size, F.cluster_start);
                    found = true;
                }
            }

            ASSERT(found);
            FlushClusterCache(state);
            //logprintf("FAT32: dirent flushed\n");

            // TODO: flush file cache when it's eventually implemented
            F.valid = false;
            state->free_file_handles += 1;

            ASSERT(state->free_file_handles <= FAT32_MAX_OPEN_FILES);
        }
    }
}

static s32 FS_Read(void* user, Filesystem_File_Handle fd, void* dst, u32 bytes) {
    auto state = (FAT32_State*)user;
    ASSERT(state);
    s32 ret = -1;

    if(fd < FAT32_MAX_OPEN_FILES) {
        if(state->files[fd].valid) {
            auto& F = state->files[fd];
            auto cluster = LoadCluster(state, F.current_cluster);

            auto local_offset = F.offset & (state->cluster_size - 1);
            auto cluster_remains = state->cluster_size - local_offset;
            u8* ptr = (u8*)dst;
            ret = 0;
            u32 file_remains = F.size - F.offset;

            while(bytes > 0 && file_remains > 0) {
                u32 copy_count = (cluster_remains < bytes) ? cluster_remains : bytes;
                copy_count = copy_count > file_remains ? file_remains : copy_count;
                memcpy(ptr, cluster + local_offset, copy_count);

                bytes       -= copy_count;
                ptr         += copy_count;
                
                ret         += copy_count;
                F.offset    += copy_count;
                file_remains = F.size - F.offset;
                cluster_remains -= copy_count;

                // If we read past the end of the cluster, load next
                if(cluster_remains == 0) {
                    F.current_cluster = GetFATEntry(state, F.current_cluster);
                    local_offset = 0;
                    cluster_remains = state->cluster_size;

                    cluster = LoadCluster(state, F.current_cluster);
                }
            }
        }
    }

    return ret;
}

static s32 FS_Write(void* user, Filesystem_File_Handle fd, const void* src, u32 bytes) {
    auto state = (FAT32_State*)user;
    ASSERT(state);
    s32 ret = -1;
    if(fd < FAT32_MAX_OPEN_FILES) {
        if(state->files[fd].valid) {
            auto& F = state->files[fd];
            auto cluster = (u8*)LoadCluster(state, F.current_cluster);
            //logprintf("FS_Write: Current cluster %x\n", F.current_cluster);

            // Offset inside the cluster
            auto cluster_offset = (F.offset % state->cluster_size);
            
            ASSERT(state->cluster_size >= cluster_offset);
            u32 cluster_remain = state->cluster_size - cluster_offset;

            ret = 0;
            if(bytes <= cluster_remain) {
                memcpy(cluster + cluster_offset, src, bytes);
                F.offset += bytes;
                ret += bytes;
                MARK_CLUSTER_CACHE_DIRTY(state);

                if(bytes == cluster_remain) {
                    auto next_cluster = NextCluster(state, F.current_cluster);
                    F.current_cluster = next_cluster;
                }

                if(F.offset > F.size) {
                    F.size = F.offset;
                }
            } else {
                auto src8 = (u8*)src;
                memcpy(cluster + cluster_offset, src, cluster_remain);
                MARK_CLUSTER_CACHE_DIRTY(state);
                F.offset += cluster_remain;
                bytes -= cluster_remain;
                src8 += cluster_remain;
                ret += cluster_remain;

                F.current_cluster = NextCluster(state, F.current_cluster);
                ASSERT(F.current_cluster != 0);
                while(bytes > 0) {
                    auto bytes_remain = bytes % state->cluster_size;
                    if(bytes_remain == 0) {
                        bytes_remain = state->cluster_size;
                    }
                    cluster_offset = 0;
                    cluster_remain = state->cluster_size;
                    //logprintf("FS_Write: Current cluster ext %x bytes %x\n", F.current_cluster, bytes);
                    cluster = (u8*)LoadCluster(state, F.current_cluster);

                    memcpy(cluster, src8, bytes_remain);
                    MARK_CLUSTER_CACHE_DIRTY(state);

                    F.offset += bytes_remain;
                    bytes -= bytes_remain;
                    src8 += bytes_remain;
                    ret += bytes_remain;

                    if(bytes_remain == cluster_remain) {
                        //logprintf("FS_Write: Allocating new cluster\n");
                        F.current_cluster = NextCluster(state, F.current_cluster);
                        ASSERT(F.current_cluster != 0);
                    }

                    if(F.offset > F.size) {
                        F.size = F.offset;
                    }
                }
            }
        }
    }

    return ret;
}

static s32 FS_Tell(void* user, Filesystem_File_Handle fd) {
    auto state = (FAT32_State*)user;
    ASSERT(state);
    s32 ret = -1;

    if(fd < FAT32_MAX_OPEN_FILES) {
        if(state->files[fd].valid) {
            auto& F = state->files[fd];
            ret = (F.offset & 0x7FFFFFFF);
        }
    }

    return ret;
}

static s32 FS_Seek(void* user, Filesystem_File_Handle fd, whence_t whence, s32 position) {
    auto state = (FAT32_State*)user;
    ASSERT(state);
    s32 ret = -1;

    if(fd < FAT32_MAX_OPEN_FILES) {
        if(state->files[fd].valid) {
            auto& F = state->files[fd];
            s32 new_offset;
            s32 soffset = (s32)(F.offset & 0x7FFFFFFF);
            s32 ssize = (s32)(F.size & 0x7FFFFFFF);
            switch(whence) {
                case whence_t::SET:
                new_offset = position;
                break;
                case whence_t::CUR:
                new_offset = soffset + position;
                break;
                case whence_t::END:
                new_offset = ssize + position;
                break;
                default:
                // Trigger an error
                new_offset = F.size;
                break;
            }
            if(new_offset < 0) {
                new_offset = 0;
            }
            if(new_offset < ssize) {
                F.current_cluster = F.cluster_start;
                // Walk the list
                u32 cluster_size = state->cluster_size;
                F.offset = new_offset;
                u32 remains = new_offset;
                while(remains > cluster_size) {
                    F.current_cluster = GetFATEntry(state, F.current_cluster);
                    remains -= cluster_size;
                }
            } else {
                // EOF
            }
        } else {
            //logprintf("SEEK:: non-valid fd\n");
        }
    } else {
        //logprintf("SEEK:: bad fd\n");
    }

    return ret;
}

static s32 FS_Sync(void* user) {
    auto state = (FAT32_State*)user;
    ASSERT(state);

    FlushFATPage(state);
    FlushClusterCache(state);

    return 0;
}

static int FS_EOF(void* user, Filesystem_File_Handle fd) {
    int ret = -1;
    auto state = (FAT32_State*)user;
    ASSERT(state);

    if(fd >= 0 && fd < FAT32_MAX_OPEN_FILES) {
        auto& F = state->files[fd];
        if(F.valid) {
            ret = F.offset == F.size ? 1 : 0;
        }
    }

    return ret;
}

static bool IsWriteProtected(FAT32_State* fs) {
    bool ret = false;
    u8 fat_old[512];
    u8 fat_new[512];
    u8 wrtest[512];

    s32 rd, wr;

    rd = Volume_Read_Blocks(fs->vol, fat_old, fs->sector_fat0, 1);
    if(rd == 1) {
        for(int i = 0; i < 512; i++) {
            wrtest[i] = 0xCD;
        }
        wr = Volume_Write_Blocks(fs->vol, wrtest, fs->sector_fat0, 1);
        if(wr == 1) {
            rd = Volume_Read_Blocks(fs->vol, fat_new, fs->sector_fat0, 1);
            ASSERT(rd == 1);
            for(int i = 0; i < 512 && !ret; i++) {
                if(fat_new[i] != wrtest[i]) {
                    //logprintf("Byte [%d] didn't match: %x != %x\n", i, fat_new[i], wrtest[i]);
                    ret = true;
                }
            }
            if(!ret) {
                Volume_Write_Blocks(fs->vol, fat_old, fs->sector_fat0, 1);
            }
        } else {
            logprintf("FAT32: Couldn't write volume %d, assuming write protection\n", fs->vol);
            ret = true;
        }
    } else {
        logprintf("FAT32: Write protection check failed on volume %d\n", fs->vol);
    }

    return ret;
}

static bool FS_Probe(Volume_Handle handle, void** user) {
    bool ret = false;
    u8 buf_bootsect[512];
    u8 buf_infosect[512];
    u8 buf_fat[512];
    auto bootsect = (FAT32_Boot_Sector*)buf_bootsect;
    auto infosect = (FAT32_Info_Sector*)buf_infosect;

    //logprintf("Testing volume #%d for FAT32 presence\n", handle);

    if(Volume_Read_Blocks(handle, buf_bootsect, 0, 1) > 0) {
        u8 buf_oem[9];
        memcpy(buf_oem, bootsect->oem, 8);
        buf_oem[8] = 0;
        //logprintf("\tOEM: %s\n", buf_oem);
        //logprintf("\tSector size: %d\n\tSectors per cluster: %d\n\tCount of FATs: %d\n\tTotal sectors: %d (%d)\n\tSectors per FAT: %d\n\tInfosector: %x\n",
            //bootsect->sector_size, bootsect->sectors_per_cluster, bootsect->count_fat, bootsect->total_sectors, bootsect->total_sectors32, bootsect->sectors_per_fat32, bootsect->sector_infosector);
        
        if(Volume_Read_Blocks(handle, buf_infosect, bootsect->sector_infosector, 1) > 0) {
            //logprintf("\tInfosector is present:\n");
            //logprintf("\t\tSignature: %x\n\t\tSignature 2: %x\n\t\tFree clusters: %x\n",
            //infosect->signature, infosect->sector_signature, infosect->free_data_clusters);

            // Bootsect signature is okay
            bool bs_signature = (buf_bootsect[510] == 0x55) && (buf_bootsect[511] == 0xAA);
            // Infosect signatures are okay
            bool is_signature = infosect->signature == 0x41615252 && infosect->sector_signature == 0x61417272;
            bool version_ok = bootsect->version == 0x0000;

            if(Volume_Read_Blocks(handle, buf_fat, bootsect->count_reserved, 1) > 0) {
                u32* fat = (u32*)buf_fat;
                u32 cluster0 = fat[0];
                u32 cluster0eoc = fat[1];

                auto fat_id = bootsect->media;
                u32 cluster0_expected = 0x0FFFFF00 | fat_id;

                bool cluster0_ok = (cluster0 == cluster0_expected) && (cluster0eoc == CLUSTER_EOC);

                ret = bs_signature && is_signature && version_ok && cluster0_ok;

                if(ret) {
                    auto state = (FAT32_State*)kmalloc(sizeof(FAT32_State));
                    ASSERT(state);
                    auto sector_size = bootsect->sector_size;
                    if(sector_size != 512) {
                        logprintf("Sector size is %d\n", sector_size);
                    }
                    state->vol = handle;
                    state->sectors_per_cluster = bootsect->sectors_per_cluster;
                    state->sector_fat0 = bootsect->count_reserved;
                    state->sector_info = bootsect->sector_infosector;
                    state->sector_count = bootsect->total_sectors == 0 ? bootsect->total_sectors32 : bootsect->total_sectors;
                    state->cluster_root_dir = bootsect->cluster_root_directory;
                    // NOTE: hard coded block size
                    state->cluster_size = bootsect->sectors_per_cluster * 512;
                    // NOTE: Make sure that cluster size is a power of two
                    // NOTE: FAT spec ensures that sectors_per_cluster is a power of two so
                    // this check is unnecessary
                    ASSERT((state->cluster_size & (state->cluster_size - 1)) == 0);

                    auto sect_per_fat = (bootsect->sectors_per_fat == 0) ? bootsect->sectors_per_fat32 : bootsect->sectors_per_fat;
                    state->sectors_per_fat = sect_per_fat;
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
                    state->cluster_cache = (u8*)kmalloc(state->sectors_per_cluster * sector_size);
                    // Put cluster 0 into cache
                    state->cluster_cache_dirty = false;
                    state->cluster_cache_index = 0;
                    LoadCluster(state, Virtual_Cluster_Index(0));

                    if(IsWriteProtected(state)) {
                        logprintf("FAT32: volume %d is write protected\n", handle);
                        state->write_protected = true;
                    } else {
                        state->write_protected = false;
                    }
                }
            }
        } else {
            logprintf("FAT32: Failed to read the information sector\n");
        }
    } else {
        logprintf("FAT32: Failed to read sector 0\n");
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
    .EOF = FS_EOF,
};

static Filesystem_Descriptor* Register() {
    return &fsds;
}

REGISTER_FILESYSTEM(Register);