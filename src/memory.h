#ifndef KERNEL_MEMORY_H
#define KERNEL_MEMORY_H

void Mem_Init(void* base, u32 length);

#define MEM_POOL_KERNEL (0)
#define MEM_POOL_USER   (1)

void* pmalloc(int pool, u32 size);
void pfree(int pool, void* addr);

#define kmalloc(size) pmalloc(MEM_POOL_KERNEL, size)
#define kfree(addr) pfree(MEM_POOL_KERNEL, addr)

#endif /* KERNEL_MEMORY_H */
