.globl print
.type print, @function
print:
    pushl %ebp
    mov %esp, %ebp
    push %esi

    mov 12(%esp), %esi
    mov $0x0006, %eax
    int $0x80

    pop %esi
    popl %ebp
    ret

.globl open
.type open, @function
open:
    pushl %ebp
    mov %esp, %ebp
    push %ebx

    mov 12(%esp), %ebx
    mov 16(%esp), %edx
    mov 20(%esp), %ecx
    mov $0x0002, %eax
    int $0x80

    pop %ebx
    popl %ebp
    ret

.globl close
.type close, @function
close:
    pushl %ebp
    mov %esp, %ebp
    push %ebx

    mov 12(%esp), %ebx
    mov $0x0003, %eax
    int $0x80

    pop %ebx
    popl %ebp
    ret

.globl read
.type read, @function
read:
    pushl %ebp
    mov %esp, %ebp
    push %ebx
    push %edi

    mov 16(%esp), %edx
    mov 20(%esp), %edi
    mov 24(%esp), %ebx
    mov 28(%esp), %ecx
    xorl %eax, %eax
    int $0x80

    pop %edi
    pop %ebx
    popl %ebp
    ret

.globl write
.type write, @function
write:
    pushl %ebp
    mov %esp, %ebp
    push %ebx
    push %esi

    mov 16(%esp), %edx
    mov 20(%esp), %esi
    mov 24(%esp), %ebx
    mov 28(%esp), %ecx
    mov $0x0001, %eax
    int $0x80

    pop %esi
    pop %ebx
    popl %ebp
    ret

.globl seek
.type seek, @function
seek:
    pushl %ebp
    mov %esp, %ebp
    push %ebx

    mov 12(%esp), %edx
    mov 16(%esp), %ebx
    mov 20(%esp), %ecx
    mov $0x0004, %eax
    int $0x80

    pop %ebx
    popl %ebp
    ret

.globl tell
.type tell, @function
tell:
    pushl %ebp
    mov %esp, %ebp

    mov 8(%esp), %edx
    mov $0x0005, %eax
    int $0x80

    popl %ebp
    ret

.globl poll_kbd
.type poll_kbd, @function
poll_kbd:
    pushl %ebp
    mov %esp, %ebp

    mov 8(%esp), %ecx
    mov 12(%esp), %edx
    mov $0x0008, %eax
    int $0x80

    popl %ebp
    ret
