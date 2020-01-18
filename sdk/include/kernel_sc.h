#ifndef KERNEL_SYSCALL_H
#define KERNEL_SYSCALL_H

#include "kernel/virtkeys.h"

#ifdef __cplusplus
#define CLINK extern "C"
#endif

#define O_RDONLY (1)
#define O_WRONLY (2)
#define O_RDWR (3)
#define O_CREAT (4)

#define SEEK_SET (0)
#define SEEK_CUR (1)
#define SEEK_END (2)

CLINK int read(int fd, void* buf, unsigned long size, unsigned long count);
CLINK int write(int fd, const void* buf, unsigned long size, unsigned long count);
CLINK int open(unsigned long volume, const char* path, int mode);
CLINK void close(int fd);
CLINK void seek(int fd, int whence, int position);
CLINK int tell(int fd);
CLINK void print(const char* zstring);
CLINK void printch(char ch);
CLINK int poll_kbd(int id, Keyboard_Event* buf);

#endif /* KERNEL_SYSCALL_H */

