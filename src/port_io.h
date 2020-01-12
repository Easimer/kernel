#ifndef KERNEL_PORTIO_H
#define KERNEL_PORTIO_H

#include "common.h"

CLINK void outb(u32 addr, u32 value);
CLINK void outw(u32 addr, u32 value);
CLINK void outd(u32 addr, u32 value);

CLINK u8 inb(u32 addr);
CLINK u16 inw(u32 addr);
CLINK u32 ind(u32 addr);

CLINK void insd(u16 addr, u32* buffer, u32 count);

#endif /* KERNEL_PORTIO_H */
