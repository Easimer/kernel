#ifndef KERNEL_PC_VGA_H
#define KERNEL_PC_VGA_H

void PCVGA_Init();

void PCVGA_PrintNotice(const char* notice);
void PCVGA_PutChar(char c);

#endif
