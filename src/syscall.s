global sys_call

extern t_sys_call

sys_call:
    cld
    push ds
    push es
    push fs
    push gs
    push edi
    push esi
    push ebx
    push edx
    push ecx
    push eax
    mov eax, 0x10
    mov ds, eax
    mov es, eax
    mov fs, eax
    mov gs, eax
    call t_sys_call
    add esp, 16
    pop esi
    pop edi
    pop gs
    pop fs
    pop es
    pop ds
    iret
