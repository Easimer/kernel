#ifndef KERNEL_PS2_H
#define KERNEL_PS2_H

#include "common.h"

void PS2_Setup();

bool PS2_ResetDevice(u32 dev);
void PS2_SendToDevice(u32 dev, u8 value);
bool PS2_ReadDataWithTimeout(u32 ticks, u8* value);
bool PS2_CanReadData();
bool PS2_CanSendData();

#endif /* KERNEL_PS2_H */
