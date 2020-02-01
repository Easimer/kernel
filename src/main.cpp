#include "common.h"
#include "logging.h"
#include "pc_vga.h"
#include "uart.h"
#include "multiboot2.h"
#include "utils.h"
#include "memory.h"
#include "interrupts.h"
#include "timer.h"
#include "ps2.h"
#include "pci.h"
#include "disk.h"
#include "volumes.h"
#include "exec.h"
#include "vm.h"
#include "pfalloc.h"

extern "C" void _init();
extern "C" void _fini();

extern "C" u32* boot_page_directory;

extern "C" void kmain(u32 magic, const MB2_Header* mb2) {
    MM_Init();
    SSE_Setup();
    Log_Init();
    PCVGA_Init();
    UART_Setup(PORT_COM1);

    Interrupts_Setup();
    Timer_Setup();

    logprintf("Hello World! %x\n", magic);

    if(IS_MULTIBOOT2_BOOT()) {
        MB2_Parse(mb2);
    } else {
        ASSERT(!"Didn't boot from a Multiboot2 bootloader");
    }

    PS2_Setup();

    // Run driver registration code
    _init();

    // Enumerate PCI devices
    PCI_Enumerate();

    // Probe partitions
    Disk_Partition_Probe();

    // Initialize volume manager
    Volume_Init();

    logprintf("Loading COMMAND.EXE\n");
    const char* argv[] = {"/COMMAND.EXE"};
    int ret = Execute_Program(3, "/COMMAND.EXE", 1, argv);
    logprintf("COMMAND.EXE returned with code %d\n", ret);

    while(1) {
    }

    _fini();
}

// Stack protector
u32 __stack_chk_guard = 0xe2dee396;
extern "C" void __stack_chk_fail(void) {
    ASSERT(!"Stack smashing detected");
}

extern "C" void __stack_chk_fail_local(void) {
    ASSERT(!"Stack smashing detected");
}
