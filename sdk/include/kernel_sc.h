#ifndef KERNEL_SYSCALL_H
#define KERNEL_SYSCALL_H

#ifdef __cplusplus
#define CLINK extern "C"
#endif

CLINK int read(int fd, void* buf, unsigned long size, unsigned long count);
CLINK int write(int fd, const void* buf, unsigned long size, unsigned long count);
CLINK int open(unsigned long volume, const char* path, int mode);
CLINK void close(int fd);
CLINK void seek(int fd, int whence, int position);
CLINK int tell(int fd);
CLINK void print(const char* zstring);
CLINK void printch(char ch);

#endif /* KERNEL_SYSCALL_H */

