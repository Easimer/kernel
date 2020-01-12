#include "common.h"
#include "timer.h"
#include "port_io.h"
#include "interrupts.h"

static u32 ticks;

static void TimerHandler(const Registers* regs) {
    ticks++;
}

void Timer_Setup() {
    ticks = 0;

    u16 divisor = 11931;
    u8 l = divisor & 0xFF;
    u8 h = (divisor >> 8) & 0xFF;

    Interrupts_Register_Handler(IRQ0, TimerHandler);

    outb(0x43, 0x36);
    outb(0x40, l);
    outb(0x40, h);
}

void Sleep(u32 millis) {
    // TODO:
}

u32 TicksElapsed() {
    return ticks;
}