#ifndef KERNEL_VOLUMES_H
#define KERNEL_VOLUMES_H

#include "common.h"
//#include "disk.h"

using Volume_Handle = u32;

struct Volume_Descriptor {
    u32 disk;
    u32 offset, length;
    void* user;
};

Volume_Handle Volume_Register(const Volume_Descriptor* vol);
s32 Volume_Read_Blocks(Volume_Handle vol, void* buffer, u32 offset, u32 count);
s32 Volume_Write_Blocks(Volume_Handle vol, const void* buffer, u32 offset, u32 count);

struct Filesystem;

Filesystem* Volume_Get_Filesystem(Volume_Handle vol);

using Filesystem_File_Handle = s32;
enum mode_t {
    O_RDONLY = 0,
    O_WRONLY = 1,
    O_RDWR = 2,
};

enum class whence_t {
    SET, CUR, END
};

using Filesystem_Open = Filesystem_File_Handle (*)(void* user, const char* path, mode_t flags);
using Filesystem_Close = void (*)(void* user, Filesystem_File_Handle fd);
using Filesystem_Read = s32 (*)(void* user, Filesystem_File_Handle fd, void* dst, s32 bytes);
using Filesystem_Write = s32 (*)(void* user, Filesystem_File_Handle fd, const void* src, s32 bytes);
using Filesystem_Tell = s32 (*)(void* user, Filesystem_File_Handle fd);
using Filesystem_Seek = s32 (*)(void* user, Filesystem_File_Handle fd, whence_t whence, u32 position);
using Filesystem_Sync = s32 (*)(void* user);

using Filesystem_Probe = bool (*)(Volume_Handle handle, void** user);

struct Filesystem_Descriptor {
    const char* Name;
    Filesystem_Probe Probe;
    Filesystem_Open Open;
    Filesystem_Close Close;
    Filesystem_Read Read;
    Filesystem_Write Write;
    Filesystem_Tell Tell;
    Filesystem_Seek Seek;
    Filesystem_Sync Sync;
};

using Filesystem_Register =  Filesystem_Descriptor* (*)();

void Filesystem_Register_Filesystem(Filesystem_Register init);
void Volume_Detect_Filesystems();

struct Filesystem_Register_Proxy {
    Filesystem_Register_Proxy(Filesystem_Register init) {
        Filesystem_Register_Filesystem(init);
    }
};

#define REGISTER_FILESYSTEM(init) static Filesystem_Register_Proxy __fsproxy(init)

#endif /* KERNEL_VOLUMES_H */