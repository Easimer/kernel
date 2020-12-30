#include "common.h"
#include "pfalloc.h"
#include "utils.h"
#include "logging.h"

enum Memory_Region_Type {
    MRT_Unused = 0,
    MRT_Unprocessed,
    MRT_Free,
    MRT_Kernel,
    MRT_Program,
    MRT_Last
};

struct Memory_Region {
    constexpr Memory_Region() : type(MRT_Unused), addr_first(0), addr_last(0), prev(NULL), next(NULL), program_id(0) {}
    Memory_Region_Type type;
    u32 addr_first, addr_last;
    Memory_Region *prev, *next;
    u32 program_id;

    inline u32 size() const noexcept {
        return addr_last - addr_first + 1;
    }
};

#define MEMORY_REGIONS_MAX (256)
static Memory_Region gMemoryRegionPool[MEMORY_REGIONS_MAX];
static Memory_Region *gMemoryRegions, *gMemoryRegionsLast;

// Symbols defined in linker.ld
extern "C" u32 _kernel_start;
extern "C" u32 _kernel_end;

static Memory_Region* FindUnusedRegion() {
    Memory_Region* ret = NULL;

    for(int i = 0; i < MEMORY_REGIONS_MAX && ret == NULL; i++) {
        auto& cur = gMemoryRegionPool[i];
        if(cur.type == MRT_Unused) {
            cur.type = MRT_Unprocessed;
            ret = &cur;
        }
    }

    return ret;
}

static void PFA_DebugPrint() {
    auto cur = gMemoryRegions;
    logprintf("Memory regions:\n");
    while(cur) {
        logprintf("\t(%d) %x -> %x\n", cur->type, cur->addr_first, cur->addr_last);
        cur = cur->next;
    }
}

// Tries inserting the region
static void InsertRegion(const Memory_Region& region) {
    bool inserted = false;

    auto cur = gMemoryRegions;
    ASSERT(gMemoryRegions);

    while(cur) {
        bool contains_head = cur->addr_first <= region.addr_first && region.addr_first < cur->addr_last;
        //bool contains_tail = cur->addr_last >= region.addr_last;
        bool contains_tail = cur->addr_first < region.addr_last && region.addr_last <= cur->addr_last;

        ASSERT((contains_head && contains_tail) || (!contains_head && !contains_tail));

        if(contains_head && contains_tail) {
            bool first_addr_overlapped = cur->addr_first == region.addr_first;
            bool last_addr_overlapped = cur->addr_last == region.addr_last;
            if(first_addr_overlapped && last_addr_overlapped) {
                // Replace cur region with arg region
                cur->type = region.type;
                inserted = true;
            } else {
                Memory_Region* new_region;
                new_region = FindUnusedRegion();
                new_region->addr_first = region.addr_first;
                new_region->addr_last = region.addr_last;
                new_region->type = region.type;

                if(first_addr_overlapped) {
                    // Argument and current regions' first addresses match
                    // We must insert the new element between cur->prev and cur
                    
                    // Link cur->prev to new_region
                    new_region->prev = cur->prev;
                    if(new_region->prev) {
                        new_region->prev->next = new_region;
                    } else {
                        // No previous region; update the region list head
                        gMemoryRegions = new_region;
                    }

                    // Link new_region to cur
                    cur->prev = new_region;
                    new_region->next = cur;
                    // Resize cur region
                    cur->addr_first = region.addr_last + 1;
                } else if(last_addr_overlapped) {
                    // Argument and current regions' last addresses match
                    // We must insert the new element between cur and cur->next

                    // Link cur->next to new_region
                    new_region->next = cur->next;
                    if(new_region->next) {
                        new_region->next->prev = new_region;
                    } else {
                        // No next subsequent region; update the region list tail
                        gMemoryRegionsLast = new_region;
                    }
                    // Link cur to new_region
                    cur->next = new_region;
                    new_region->prev = cur;
                    // Resize cur region
                    cur->addr_last = region.addr_first - 1;
                } else {
                    auto cur_prev = cur->prev;
                    auto cur_next = cur->next;
                    auto pre = FindUnusedRegion();
                    auto post = FindUnusedRegion();
                    ASSERT(pre && post);
                    ASSERT(pre != cur && cur != post);

                    auto A = cur->addr_first;
                    auto B = region.addr_first;
                    auto C = region.addr_last;
                    auto D = cur->addr_last;

                    pre->prev = cur_prev;
                    post->next = cur_next;
                    cur->prev = pre; pre->next = cur;
                    cur->next = post; post->prev = cur;
                    if(cur_prev) cur_prev->next = pre;
                    if(cur_next) cur_next->prev = post;

                    pre->addr_first = A;
                    pre->addr_last = B - 1;
                    cur->addr_first = B;
                    cur->addr_last = C;
                    post->addr_first = C + 1;
                    post->addr_last = D;
                    pre->type = cur->type;
                    post->type = cur->type;
                    cur->type = region.type;

                    if(cur == gMemoryRegions) {
                        gMemoryRegions = pre;
                    }
                    if(cur == gMemoryRegionsLast) {
                        gMemoryRegionsLast = post;
                    }
                }
                inserted = true;
            }
            break;
        }

        cur = cur->next;
    }

    if(!inserted) {
        logprintf("Failed to insert region [%x, %x]\n", region.addr_first, region.addr_last);
        PFA_DebugPrint();
    }

    ASSERT(inserted);
}

static void TryMergingFreeRegions(Memory_Region* region) {
    ASSERT(region->type == MRT_Free);
    if(region->prev && region->prev->type == MRT_Free) {
        auto prev = region->prev;
        prev->addr_last = region->addr_last;
        region->type = MRT_Unused;
        prev->next = region->next;
        if(region->next) {
            region->next->prev = prev;
        }
        region->prev = region->next = NULL;
        // Recurse
        TryMergingFreeRegions(prev);
    } else if(region->next && region->next->type == MRT_Free) {
        auto next = region->next;
        next->addr_first = region->addr_first;
        region->type = MRT_Unused;
        next->prev = region->prev;
        if(region->prev) {
            region->prev->next = next;
        }
        region->prev = region->next = NULL;
        // Recurse
        TryMergingFreeRegions(next);
    }
}

static void RemoveRegion(Memory_Region* region) {
    region->type = MRT_Free;
    TryMergingFreeRegions(region);
}

void PFA_Init_InsertFree(u32 addr, u32 len) {
    Memory_Region region;

    ASSERT(len > 0);

    region.type = MRT_Free;
    region.addr_first = addr;
    region.addr_last = addr + len - 1;
    //region.next = region.prev = NULL;

    InsertRegion(region);
}

void PFA_Init(u32 last_physical_address) {
    // Clear region descriptors
    for(int i = 0; i < MEMORY_REGIONS_MAX; i++) {
        gMemoryRegionPool[i].type = MRT_Unused;
        gMemoryRegionPool[i].addr_first = 0;
        gMemoryRegionPool[i].addr_last = 0;
        gMemoryRegionPool[i].prev = NULL;
        gMemoryRegionPool[i].next = NULL;
    }

    // Mark entire physical address space as unprocessed
    // MB2_Parse will tell us what free regions there are
    gMemoryRegionPool[0].addr_last = last_physical_address - 1;
    gMemoryRegionPool[0].type = MRT_Unprocessed;

    gMemoryRegions = &gMemoryRegionPool[0];
    gMemoryRegionsLast = &gMemoryRegionPool[0];

    logprintf("pfalloc: memory is [0x0, %x]\n", last_physical_address);
}

void PFA_PostInit() {
    // Called after the memory regions has been mapped
    // Insert region containing the kernel image
    Memory_Region kernel_image;

    kernel_image.type = MRT_Kernel;
    kernel_image.addr_first = (u32)(&_kernel_start) - 0xC0000000;
    kernel_image.addr_last = ((u32)(&_kernel_end + 4095) & 0xFFFFF000) - 1 - 0xC0000000; // NOTE: check if 4095 is correct here
    InsertRegion(kernel_image);

    // Insert region containing the VGA framebuffer
    kernel_image.addr_first = 0xB8000;
    kernel_image.addr_last = 0xB8FFF;
    InsertRegion(kernel_image);

    PFA_DebugPrint();
}

bool PFA_Alloc(u32 *addr, u32 program_id, u32 size) {
    ASSERT(size > 0);

    *addr = NULL;

    if(size > 0) {
        ASSERT((size & 4095) == 0);
        
        Memory_Region region;
        Memory_Region* candidate = NULL;
        auto cur = gMemoryRegions;
        while(cur) {
            auto cur_size = cur->size();
            if(cur->type == MRT_Free && cur_size >= size) {
                if(!candidate || candidate->size() < cur->size()) {
                    candidate = cur;
                }
            }
            cur = cur->next;
        }

        if(candidate) {
            region.addr_first = candidate->addr_first;
            region.addr_last = region.addr_first + size - 1;
            region.program_id = program_id;
            region.type = program_id == 0 ? MRT_Kernel : MRT_Program;
            InsertRegion(region);

            *addr = region.addr_first;
        } else {
            logprintf("pfalloc: out of memory\n");
            return false;
        }
    }

    return true;
}

bool PFA_Alloc(u32 *addr, u32 size) {
    return PFA_Alloc(addr, 0, size);
}

void PFA_Free(u32 addr) {
    u32 addr_u32 = (u32)addr;
    auto cur = gMemoryRegions;

    while(cur) {
        if(cur->addr_first == addr_u32) {
            ASSERT(cur->type != MRT_Free);
            RemoveRegion(cur);
            break;
        }
        cur = cur->next;
    }
}

void PFA_FreeAll(u32 program_id);
