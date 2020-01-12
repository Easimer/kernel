#include "common.h"
#include "utils.h"
#include "multiboot2.h"
#include "logging.h"
#include "memory.h"

#define NEXT_TAG_UNALIGNED(tag_hdr) (((u8*)(tag_hdr)) + tag_hdr->size)
#define JUMP_NEXT_TAG(tag_hdr) tag_hdr = (const MB2_Tag_Header*)((u32)(NEXT_TAG_UNALIGNED(tag_hdr) + 7) & -8)

static void MB2_Parse_MemMap(const MB2_Tag_Memory_Map* mm) {
    auto entry_size = mm->entry_size;
    const MB2_Tag_Memory_Map_Entry* end = (MB2_Tag_Memory_Map_Entry*)((u8*)mm + mm->hdr.size);

    auto entry = (const MB2_Tag_Memory_Map_Entry*)(mm + 1);

    logprintf("Parsing memory map of size %x\n", mm->hdr.size);

    void* base_max = NULL;
    u32 len_max = 0;

    while(entry != end) {
        if(entry->type == 1) {
            u32 addr = (entry->base_addr & 0xFFFFFFFF);
            u32 len = (entry->length & 0xFFFFFFFF);
            logprintf("Memory section base=%x len=%x bytes\n", addr, len);

            if(len > len_max) {
                len_max = len;
                base_max = (void*)addr;
            }
        }
        entry++;
    }

    ASSERT(base_max && len_max != 0);
    Mem_Init(base_max, len_max);
}

void MB2_Parse(const MB2_Header* hdr) {
    auto tag_hdr = (const MB2_Tag_Header*)(hdr + 1);
    logprintf("Parsing Multiboot2 info\n");

    while(tag_hdr->type != 0) {
        logprintf("Tag type=%d size=%x\n", tag_hdr->type, tag_hdr->size);
        switch(tag_hdr->type) {
            case MB2_TAG_MEMMAP:
                MB2_Parse_MemMap((const MB2_Tag_Memory_Map*)tag_hdr);
                break;
        }
        JUMP_NEXT_TAG(tag_hdr);
    }

    logprintf("Parsing Multiboot2 info ended\n");
}
