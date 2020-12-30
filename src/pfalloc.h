#ifndef KERNEL_PFALLOC_H
#define KERNEL_PFALLOC_H

// Page Frame Allocation

#include "common.h"

void PFA_Init(u32 last_physical_address);
void PFA_Init_InsertFree(u32 addr, u32 len);
void PFA_PostInit();

bool PFA_Alloc(u32 *addr, u32 program_id, u32 size);
bool PFA_Alloc(u32 *addr, u32 size);
void PFA_Free(u32 addr);
void PFA_FreeAll(u32 program_id);

#endif /* KERNEL_PFALLOC_H */
