#ifndef KERNEL_TIMER_H
#define KERNEL_TIMER_H

#include "common.h"

void Timer_Setup();
void Sleep(u32 millis);
u32 TicksElapsed();

#endif /* KERNEL_TIMER_H */