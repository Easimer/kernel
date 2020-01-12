BITS 32
CPU 686

section .multiboot
align 8
    MB2_MAGIC equ 0xE85250D6
    MB2_HDRLEN equ (.multiboot_end - .multiboot_start)

    .multiboot_start:
    dd MB2_MAGIC                            ; Magic
    dd 0                                    ; Architecture (x86)
    dd MB2_HDRLEN                           ; Header length
    dd -(MB2_MAGIC + MB2_HDRLEN)

    ;; Information Request (memory map)
    .inforeq_start:
    dw 1                                ; Type = 1 (info req)
    dw 0                                ; Flags = 0
    dd (.inforeq_end - .inforeq_start)  ; Tag length
    dd 6                                ; Requests (N=1, memory map)
    .inforeq_end:
    align 8
    ;; Null Tag
    dw 0
    dw 0
    dd 8
    .multiboot_end:

section .bss
    align 16
    stack_bottom:
    resb 16384 ; 16 KiB
    stack_top:

section .text
global _start

_start:
    mov esp, stack_top

    push ebx
    push eax
    extern kmain
    call kmain

    cli
    .hang:
    hlt
    jmp .hang
    .end:
    
