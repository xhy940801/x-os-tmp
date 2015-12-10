global sys_call

extern t_sys_call

sys_call:
    cld
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
    push eax
    mov eax, 0x2b
    mov ds, eax
    mov es, eax
    mov fs, eax
    mov gs, eax
    pop eax
    pop edi
    pop esi
    iret
