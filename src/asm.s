global _sti
global _cli
global _std
global _cld
global _outb
global _outw
global _inb
global _inw

global _ltr

_sti:
    sti
    ret

_cli:
    cli
    ret

_std:
    std
    ret

_cld:
    cld
    ret

_outb:
    mov edx, [esp + 4]
    mov eax, [esp + 8]
    out dx, al
    ret

_outw:
    mov edx, [esp + 4]
    mov eax, [esp + 8]
    out dx, ax
    ret

_inb:
    mov edx, [esp + 4]
    mov eax, [esp + 8]
    in  al, dx
    ret

_inw:
    mov edx, [esp + 4]
    mov eax, [esp + 8]
    in  ax, dx
    ret

_ltr:
    mov eax, [esp + 4]
    ltr ax
    ret
