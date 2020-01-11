#include "common.h"
#include "logging.h"
#include "pc_vga.h"

extern "C" void kmain(void) {
    Log_Init();
    PCVGA_Init();

    Log_LogString("Hello World!\n");
}
