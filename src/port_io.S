; void outb(u16 addr, u8 val)
global outb
outb:
    ; addr -> edx, val -> eax
    mov edx, DWORD [esp + 4]
    mov eax, DWORD [esp + 8]
    out dx, al
    ret

; void outw(u16 addr, u16 val)
global outw
outw:
    ; addr -> edx, val -> eax
    mov edx, DWORD [esp + 4]
    mov eax, DWORD [esp + 8]
    out dx, ax
    ret

; void outd(u16 addr, u32 val)
global outd
outd:
    ; addr -> edx, val -> eax
    mov edx, DWORD [esp + 4]
    mov eax, DWORD [esp + 8]
    out dx, eax
    ret

; u8 inb(u16 addr)
global inb
inb:
    ; addr -> edx, val -> eax
    mov edx, DWORD [esp + 4]
    xor eax, eax
    in al, dx
    ret


; u16 inb(u16 addr)
global inw
inw:
    ; addr -> edx, val -> eax
    mov edx, DWORD [esp + 4]
    xor eax, eax
    in ax, dx
    ret

; u32 ind(u16 addr)
global ind
ind:
    ; addr -> edx, val -> eax
    mov edx, DWORD [esp + 4]
    xor eax, eax
    in eax, dx
    ret

