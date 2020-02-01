BITS 32
CPU 686

%define PHYS(vaddr) (vaddr - 0xC0000000)

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
    align 4096
    boot_page_directory:
    resb 4096
    boot_page_table:
    resb 4096

global boot_page_directory
global boot_page_table

section .text
global _start

extern _kernel_start
extern _kernel_end

_start:
    ; Setup paging
    xor esi, esi
    mov edi, PHYS(boot_page_table)
    mov ecx, 1023
.seek_kernel_start_loop:
    cmp esi, PHYS(_kernel_start)
    ;jl SHORT .seek_kernel_start_step
    jl SHORT .seek_kernel_start_step
    cmp esi, PHYS(_kernel_end)
    jge SHORT .finalize_paging

    ; Place PT entry
    mov edx, esi
    or edx, 0x3 ; PT_PRESENT + PT_READWRITE
    mov DWORD [edi], edx
.seek_kernel_start_step:
    add esi, 4096
    add edi, 4
    loop .seek_kernel_start_loop
.finalize_paging:
    ; Map VGA framebuffer to 0xC03FF000
    mov DWORD [PHYS(boot_page_table) + 1023 * 4], (0xB8000 | 0x3)
    ; Identity map kernel
    mov DWORD [PHYS(boot_page_directory)], (PHYS(boot_page_table) + 0x3)
    ; Map kernel to 0xC0000000
    mov DWORD [PHYS(boot_page_directory) + 768 * 4], (PHYS(boot_page_table) + 0x3)
    
    mov ecx, PHYS(boot_page_directory)
    mov cr3, ecx

    mov ecx, cr0
    or DWORD ecx, 0x80010000
    mov cr0, ecx

    ; Long jump
    mov ecx, .virtual
    jmp ecx
.virtual:
    ; Remove identity map
    mov DWORD [boot_page_directory], 0
    mov ecx, cr3
    mov cr3, ecx

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
    
