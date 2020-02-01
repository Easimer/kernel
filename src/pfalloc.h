#ifndef KERNEL_PFALLOC_H
#define KERNEL_PFALLOC_H

// Page Frame Allocation

#include "common.h"

void PFA_Init(u32 last_physical_address);
void PFA_Init_InsertFree(u32 addr, u32 len);
void PFA_PostInit();

void PFA_InsertKernel(); // NOTE: might not need this
void* PFA_Alloc(u32 program_id, u32 size);
void* PFA_AllocKernel(u32 size);
void PFA_Free(void* addr);
void PFA_FreeAll(u32 program_id);

#endif /* KERNEL_PFALLOC_H */
