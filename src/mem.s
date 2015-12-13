global on_lack_of_page

extern process_lack_page
extern get_locker_count
extern set_locker_count

on_lack_of_page:
    push ds
    push es
    push fs
    push gs

    sub esp, 4
    push eax
    push ecx
    push edx
    mov eax, [esp + 32]
    push eax
    cld
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

    call process_lack_page

    cli
    mov eax, [esp + 16]
    push eax
    call set_locker_count
    add esp, 4
    sti

    add esp, 4
    pop edx
    pop ecx
    pop eax
    add esp, 4

    pop gs
    pop fs
    pop es
    pop ds
    add esp, 4
    iret
