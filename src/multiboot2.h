#ifndef KERNEL_MULTIBOOT2_H
#define KERNEL_MULTIBOOT2_H

#include "common.h"

#define MULTIBOOT2_MAGIC (0x36D76289)
#define IS_MULTIBOOT2_BOOT() (magic == MULTIBOOT2_MAGIC)

struct MB2_Header {
    u32 total_size, reserved;
};

struct MB2_Tag_Header {
    u32 type, size;
};

struct MB2_Tag_Memory_Map {
    MB2_Tag_Header hdr;
    u32 entry_size;
    u32 entry_version;
};

#define MB2_TAG_MEMMAP (6)

struct MB2_Tag_Memory_Map_Entry {
    u64 base_addr;
    u64 length;
    u32 type;
    u32 reserved;
};

void MB2_Parse(const MB2_Header* hdr);

#endif /* KERNEL_MULTIBOOT2_H */
