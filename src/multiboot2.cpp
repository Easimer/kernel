#include "common.h"
#include "utils.h"
#include "multiboot2.h"
#include "logging.h"
#include "vm.h"
#include "pfalloc.h"

#define NEXT_TAG_UNALIGNED(tag_hdr) (((u8*)(tag_hdr)) + tag_hdr->size)
#define JUMP_NEXT_TAG(tag_hdr) tag_hdr = (const MB2_Tag_Header*)((u32)(NEXT_TAG_UNALIGNED(tag_hdr) + 7) & -8)

static void MB2_Parse_MemMap(const MB2_Tag_Memory_Map* mm) {
    u32 addr_max = 0;
    const MB2_Tag_Memory_Map_Entry* end = (MB2_Tag_Memory_Map_Entry*)((u8*)mm + mm->hdr.size);
    auto entry = (const MB2_Tag_Memory_Map_Entry*)(mm + 1);

    // Find maximum physical address
    while(entry != end) {
        if(entry->type == 1) {
            u32 addr = (entry->base_addr & 0xFFFFFFFF);
            u32 len = (entry->length & 0xFFFFFFFF);
            auto end = addr + len;

            if(end > addr_max) {
                addr_max = end;
            }
        }
        entry++;
    }

    PFA_Init(addr_max);

    // Store free regions
    entry = (const MB2_Tag_Memory_Map_Entry*)(mm + 1);

    while(entry != end) {
        if(entry->type == 1) {
            u32 addr = (entry->base_addr & 0xFFFFFFFF);
            u32 len = (entry->length & 0xFFFFFFFF);
            logprintf("Memory section base=%x len=%x bytes\n", addr, len);

            PFA_Init_InsertFree(addr, len);
        }
        entry++;
    }

    PFA_PostInit();
}

void MB2_Parse(const MB2_Header* hdr) {
    // Passed-in address is physical, we need to map it first
    u32 hdr_phys = (u32)hdr;
    // Page reserved for us
    auto off = (hdr_phys - (hdr_phys & 0xFFFFF000));
    auto vhdr = (MB2_Header*)(0xC03FD000 + off);
    auto page_end = ((u8*)vhdr + 4096);
    MM_VirtualMap((void*)0xC03FD000, hdr_phys & 0xFFFFF000);
    auto tag_hdr = (const MB2_Tag_Header*)(vhdr + 1);
    logprintf("Parsing Multiboot2 info len=%x\n", vhdr->total_size);
    // TODO: map hdr to an address

    while(tag_hdr->type != 0) {
        logprintf("Tag type=%d size=%x\n", tag_hdr->type, tag_hdr->size);
        switch(tag_hdr->type) {
            case MB2_TAG_MEMMAP:
                MB2_Parse_MemMap((const MB2_Tag_Memory_Map*)tag_hdr);
                break;
        }
        JUMP_NEXT_TAG(tag_hdr);

        // Break when we reached page boundary
        if((u8*)tag_hdr >= page_end || (u8*)tag_hdr + tag_hdr->size >= page_end) {
            break;
        }
    }

    MM_VirtualUnmap((void*)0xC03FD000);

    //logprintf("Parsing Multiboot2 info ended\n");
}
