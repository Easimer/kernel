#ifndef KERNEL_SYSCALLS_H
#define KERNEL_SYSCALLS_H

#define SYSCALL_READ            (0x0000)
// EBX=size ECX=count EDX=fd EDI=dst EAX<-bytes_read
#define SYSCALL_WRITE           (0x0001)
// EBX=size ECX=count EDX=fd ESI=src EAX<-bytes_written
#define SYSCALL_OPEN            (0x0002)
// EBX=volume ECX=mode EDX=filename EAX<-fd
#define SYSCALL_CLOSE           (0x0003)
// EBX=fd
#define SYSCALL_SEEK            (0x0004)
// EBX=whence ECX=position EDX=fd EAX<-0
#define SYSCALL_TELL            (0x0005)
// EDX=fd EAX<-position
#define SYSCALL_PRINT           (0x0006)
// ESI=zstring
#define SYSCALL_PRINTCH         (0x0007)
// EDX=char

#endif /* KERNEL_SYSCALLS_H */