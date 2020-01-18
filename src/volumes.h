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

using mode_t = int;

#define O_RDONLY (1)
#define O_WRONLY (2)
#define O_RDWR (3)
#define O_CREAT (4)

enum class whence_t {
    SET, CUR, END
};

using Filesystem_Open = Filesystem_File_Handle (*)(void* user, const char* path, mode_t flags);
using Filesystem_Close = void (*)(void* user, Filesystem_File_Handle fd);
using Filesystem_Read = s32 (*)(void* user, Filesystem_File_Handle fd, void* dst, u32 bytes);
using Filesystem_Write = s32 (*)(void* user, Filesystem_File_Handle fd, const void* src, u32 bytes);
using Filesystem_Tell = s32 (*)(void* user, Filesystem_File_Handle fd);
using Filesystem_Seek = s32 (*)(void* user, Filesystem_File_Handle fd, whence_t whence, s32 position);
using Filesystem_Sync = s32 (*)(void* user);
using Filesystem_EOF = int (*)(void* user, Filesystem_File_Handle fd);

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
    Filesystem_EOF EOF;
};

using Filesystem_Register =  Filesystem_Descriptor* (*)();

void Filesystem_Register_Filesystem(Filesystem_Register init);
void Volume_Init();

int File_Open(Volume_Handle volume, const char* path, mode_t flags);
void File_Close(int fd);
int File_Read(void* ptr, u32 size, u32 nmemb, int fd);
int File_Write(const void* ptr, u32 size, u32 nmemb, int fd);
void File_Seek(int fd, s32 offset, whence_t whence);
int File_Tell(int fd);
int File_EOF(int fd);
void Sync(Volume_Handle volume);

struct Filesystem_Register_Proxy {
    Filesystem_Register_Proxy(Filesystem_Register init) {
        Filesystem_Register_Filesystem(init);
    }
};

#define REGISTER_FILESYSTEM(init) static Filesystem_Register_Proxy __fsproxy(init)

#endif /* KERNEL_VOLUMES_H */