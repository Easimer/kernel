#ifndef KERNEL_MEMORY_H
#define KERNEL_MEMORY_H

void Mem_Init(void* base, u32 length);

void* kmalloc(u32 size);
void kfree(void* addr);

#endif /* KERNEL_MEMORY_H */
