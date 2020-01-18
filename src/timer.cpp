#include "common.h"
#include "timer.h"
#include "port_io.h"
#include "interrupts.h"

static volatile u32 ticks;

static void TimerHandler(Registers* regs) {
    (void)regs;
    ticks++;
}

const u16 Divisor = 1193; // ~1kHz

void Timer_Setup() {
    ticks = 0;

    u8 l = Divisor & 0xFF;
    u8 h = (Divisor >> 8) & 0xFF;

    Interrupts_Register_Handler(IRQ0, TimerHandler);

    outb(0x43, 0x36);
    outb(0x40, l);
    outb(0x40, h);
}

#include "logging.h"

void Sleep(u32 millis) {
    auto end = ticks + millis;

    while(ticks < end) {
        asm volatile("hlt");
    }
}

u32 TicksElapsed() {
    return ticks;
}

void SleepTicks(u32 n) {
    auto end = ticks + n;

    while(ticks < end) {
        asm volatile("hlt");
    }
}