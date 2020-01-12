#ifndef KERNEL_VOLUMES_H
#define KERNEL_VOLUMES_H

#include "common.h"
//#include "disk.h"

using Volume_File_Handle = u32;
using Volume_Handle = u32;

struct Volume_Descriptor {
    u32 disk;
    u32 offset, length;
    void* user;
};

Volume_Handle Volume_Register(const Volume_Descriptor* vol);

using Filesystem = void*;

Filesystem Volume_Get_Filesystem(Volume_Handle vol);

#endif /* KERNEL_VOLUMES_H */