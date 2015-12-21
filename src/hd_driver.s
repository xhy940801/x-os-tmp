global on_hd_interrupt_request

extern get_locker_count
extern set_locker_count
extern do_hd_irq

on_hd_interrupt_request:
    push ds
    push es
    push fs
    push gs
    push ebx
    push esi
    push edi

    sub esp, 4
    push eax
    push ecx
    push edx
    cld
    
    mov eax, 0x10
    mov ds, eax
    mov es, eax
    mov fs, eax
    mov gs, eax

    call get_locker_count
    mov [esp + 12], eax
    push 0
    call set_locker_count
    add esp, 4

    call do_hd_irq

    mov eax, [esp + 12]
    push eax
    call set_locker_count
    add esp, 4

    pop edx
    pop ecx
    pop eax
    add esp, 4

    pop edi
    pop esi
    pop ebx
    pop gs
    pop fs
    pop es
    pop ds
    iret 
