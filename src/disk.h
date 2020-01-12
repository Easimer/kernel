#ifndef KERNEL_DISK_H
#define KERNEL_DISK_H

#include "common.h"

using Disk_Op_Read = bool(*)(void* user, u32* bytes_read, void* buf, u32 siz, u32 off);
using Disk_Op_Write = bool(*)(void* user, u32* bytes_written, const void* buf, u32 siz, u32 off);
using Disk_Op_Flush = bool(*)(void* user);

struct Disk_Device_Descriptor {
    Disk_Op_Read Read;
    Disk_Op_Write Write;
    Disk_Op_Flush Flush;
};

bool Disk_Register_Device(void* user, const Disk_Device_Descriptor* desc);

#endif /* KERNEL_DISK_H */