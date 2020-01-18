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
#include "exec.h"

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

    auto fd = File_Open(0, "/LOREM.TXT", O_RDONLY);
    if(fd != -1) {
        u8 buf[33];
        while(!File_EOF(fd)) {
            auto rd = File_Read(buf, 1, 32, fd);
            if(rd != -1) {
                buf[rd] = 0;
                logprintf((char*)buf);
            }
        }
        File_Close(fd);
    }

    fd = File_Open(0, "/WRTEST.TXT", O_RDWR | O_CREAT);
    if(fd != -1) {
        u8* testdat = (u8*)kmalloc(32 * 1024);
        for(u32 i = 0; i < 32 * 1024; i++) {
            testdat[i] = '0' + (i & 7);
        }
        u32 wr;
        wr = File_Write(testdat, 1, 32 * 1024, fd);
        kfree(testdat);
        File_Close(fd);
    }

    logprintf("Syncing disk\n");
    Sync(0);

    logprintf("Loading COMMAND.EXE\n");
    const char* argv[] = {"/COMMAND.EXE"};
    int ret = Execute_Program(0, "/COMMAND.EXE", 1, argv);
    logprintf("COMMAND.EXE returned with code %d\n", ret);

    while(1) {
    }

    _fini();
}
