#ifndef KERNEL_VM_H
#define KERNEL_VM_H

void MM_Init();
bool MM_VirtualMap(void* vaddr, u32 physical);
bool MM_VirtualUnmap(void* vaddr);

// Map frame(s) somewhere into the kernel address-space
void* MM_VirtualMapKernel(u32 physical, u32 page_count = 1);

// Translate virtual address to physical address
bool MM_MapToPhysical(u32* out_phys, void* addr);

void MM_PrintDiagnostic(void* vaddr);

bool AllocatePageDirectory(u32* res);
bool FreePageDirectory(u32 pd_phys);
void SwitchPageDirectory(u32 pd_phys);

// Allocate virtual memory for a program
void* AllocateProgramMemory(u32 program_id, u32 pd, u32 size);
// Free virtual memory of a program
void FreeProgramMemory(u32 pd, void* addr);

#endif /* KERNEL_VM_H */