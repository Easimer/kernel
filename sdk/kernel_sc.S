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
