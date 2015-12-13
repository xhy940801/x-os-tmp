global sys_call

extern t_sys_call
extern get_locker_count
extern set_locker_count

sys_call:
    cld
    push ds
    push es
    push fs
    push gs
    push edi
    push esi

    sub esp, 4

    push ebx
    push edx
    push ecx
    push eax
    mov eax, 0x10
    mov ds, eax
    mov es, eax
    mov fs, eax
    mov gs, eax

    cli
    call get_locker_count
    mov [esp + 16], eax
    push 0
    call set_locker_count
    add esp, 4
    sti

    call t_sys_call

    cli
    push eax
    mov eax, [esp + 20]
    push eax
    call set_locker_count
    add esp, 4
    pop eax
    sti

    add esp, 20
    pop esi
    pop edi
    pop gs
    pop fs
    pop es
    pop ds
    mov ebx, 0
    mov ecx, 0
    mov edx, 0
    iret
