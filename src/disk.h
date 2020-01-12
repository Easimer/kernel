#ifndef KERNEL_DISK_H
#define KERNEL_DISK_H

#include "common.h"

using Disk_Op_Read = bool(*)(void* user, u32* blocks_read, void* buf, u32 block_count, u32 block_off);
using Disk_Op_Write = bool(*)(void* user, u32* blocks_read, const void* buf, u32 block_count, u32 block_off);
using Disk_Op_Flush = bool(*)(void* user);

struct Disk_Device_Descriptor {
    Disk_Op_Read Read;
    Disk_Op_Write Write;
    Disk_Op_Flush Flush;

    u32 BlockSize;
};

bool Disk_Register_Device(void* user, const Disk_Device_Descriptor* desc);
void Disk_Partition_Probe();

bool Disk_Exists(u32 disk);
u32 Disk_BlockSize(u32 disk);
s32 Disk_Read_Blocks(u32 disk, void* buf, u32 block_count, u32 block_offset);
s32 Disk_Write_Blocks(u32 disk, const void* buf, u32 block_count, u32 block_offset);

#endif /* KERNEL_DISK_H */