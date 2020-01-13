#include "common.h"
#include "logging.h"
#include "pc_vga.h"
#include "uart.h"
#include "multiboot2.h"
#include "utils.h"
#include "memory.h"
#include "interrupts.h"
#include "timer.h"
#include "pci.h"
#include "disk.h"
#include "volumes.h"

extern "C" void _init();
extern "C" void _fini();

extern "C" void kmain(u32 magic, const MB2_Header* mb2) {
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

    // Run driver registration code
    _init();

    // Enumerate PCI devices
    PCI_Enumerate();

    // Probe partitions
    Disk_Partition_Probe();

    // Detect filesystems
    Volume_Detect_Filesystems();

    logprintf("Opening /TEST.TXT!\n");
    int fd = File_Open(0, "/DIR/DIRYEY/F1", O_RDONLY);
    if(fd != -1) {
        logprintf("Closing /TEST.TXT!\n");
        File_Close(fd);
    }
    logprintf("File IO test over!\n");

    while(1) {
    }

    _fini();
}
