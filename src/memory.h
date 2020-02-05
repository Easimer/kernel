#ifndef KERNEL_MEMORY_H
#define KERNEL_MEMORY_H

void Mem_Init(void* base, u32 length);

void* kmalloc(u32 size);
void kfree(void* addr);

bool AllocatePageDirectory(u32* res);
bool FreePageDirectory(u32 pd_phys);
void SwitchPageDirectory(u32 pd_phys);

void* AllocateProgramMemory(u32 pd, u32 size);
void* AllocateProgramMemory(u32 size);
void FreeProgramMemory(u32 pd, void* size);
void FreeProgramMemory(void* size);

#endif /* KERNEL_MEMORY_H */
