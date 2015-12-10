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
    call t_sys_call
    add esp, 16
    pop edi
    pop esi
    iret
