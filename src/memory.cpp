#include "common.h"
#include "utils.h"
#include "memory.h"
#include "logging.h"
#include "pfalloc.h"
#include "vm.h"

#define KERNEL_RESERVED (8 * 1024 * 1024)

void* kmalloc(u32 size) {
    void* ret = NULL;

    ASSERT(size > 0);

    if(size > 0) {
        auto round_size = (size + 4096) & 0xFFFFF000;
        logprintf("Allocating memory for %d bytes (real size will be %d)\n", size, round_size);

        u32 phys = PFA_Alloc(round_size);
        ret = MM_VirtualMapKernel(phys);
        if(ret == NULL) {
            PFA_Free(phys);
        }
    }

    return ret;
}

void kfree(void* addr) {
    if(addr) {
        u32 phys;
        if(MM_MapToPhysical(&phys, addr)) {
            MM_VirtualUnmap(addr);
            PFA_Free(phys);
        }
    }
}