#ifndef KERNEL_VM_H
#define KERNEL_VM_H

void MM_Init();
bool MM_VirtualMap(void* vaddr, u32 physical);
bool MM_VirtualUnmap(void* vaddr);

#endif /* KERNEL_VM_H */