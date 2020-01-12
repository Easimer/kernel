#include "common.h"
#include "logging.h"
#include "pc_vga.h"
#include "uart.h"

extern "C" void kmain(void) {
    Log_Init();
    PCVGA_Init();
    UART_Setup(PORT_COM1);

    Log_LogString("Hello World!\n");
}
