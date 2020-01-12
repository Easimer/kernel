#include "common.h"
#include "logging.h"
#include "pc_vga.h"
#include "uart.h"
#include "multiboot2.h"

extern "C" void kmain(u32 magic, const MB2_Header* mb2) {
    Log_Init();
    PCVGA_Init();
    UART_Setup(PORT_COM1);

    logprintf("Hello World! %x\n", 0xDEADBEEF);

    while(1) {}
}
