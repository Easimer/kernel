#ifndef KERNEL_PROCESS_H
#define KERNEL_PROCESS_H

#include "interrupts.h"

void Scheduler_Init();
void Scheduler_Interrupt(Registers* regs);
void Scheduler_LoadInit();

#endif /* KERNEL_PROCESS_H */
