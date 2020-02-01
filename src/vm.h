#ifndef KERNEL_VM_H
#define KERNEL_VM_H

void MM_Init();
bool MM_VirtualMap(void* vaddr, u32 physical);
bool MM_VirtualUnmap(void* vaddr);

// Map frame(s) somewhere into the kernel address-space
void* MM_VirtualMapKernel(u32 physical, u32 page_count = 1);

// Translate virtual address to physical address
bool MM_MapToPhysical(u32* out_phys, void* addr);

#endif /* KERNEL_VM_H */