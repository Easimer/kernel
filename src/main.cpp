#include "common.h"
#include "logging.h"
#include "pc_vga.h"
#include "uart.h"
#include "multiboot2.h"
#include "utils.h"
#include "memory.h"

extern "C" void kmain(u32 magic, const MB2_Header* mb2) {
    Log_Init();
    PCVGA_Init();
    UART_Setup(PORT_COM1);

    logprintf("Hello World! %x\n", magic);

    if(IS_MULTIBOOT2_BOOT()) {
        MB2_Parse(mb2);
    } else {
        ASSERT(!"Didn't boot from a Multiboot2 bootloader");
    }

    void* test0 = pmalloc(MEM_POOL_KERNEL, 1024);
    void* test1 = pmalloc(MEM_POOL_KERNEL, 1024);
    void* test2 = pmalloc(MEM_POOL_USER, 1024);
    void* test3 = pmalloc(MEM_POOL_USER, 1024);

    logprintf("Kernel test: %x %x User test: %x %x\n", test0, test1, test2, test3);

    pfree(MEM_POOL_USER, test2);
    pfree(MEM_POOL_USER, test3);
    pfree(MEM_POOL_KERNEL, test1);
    pfree(MEM_POOL_KERNEL, test0);

    while(1) {}
}
