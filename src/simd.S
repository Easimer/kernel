global SSE_Setup

SSE_Setup:
    push eax
    mov eax, cr0
    and ax, 0xFFFB
    or ax, 0x02
    mov cr0, eax
    mov eax, cr4
    or ax, 3 << 9
    mov cr4, eax
    pop eax
    ret
