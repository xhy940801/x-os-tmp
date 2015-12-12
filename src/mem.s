global on_lack_of_page

extern process_lack_page

on_lack_of_page:
    push ds
    push es
    push fs
    push gs

    push eax
    push ecx
    push edx
    mov eax, [esp + 12]
    push eax
    cld
    cli
    mov eax, 0x10
    mov ds, eax
    mov es, eax
    mov fs, eax
    mov gs, eax
    call process_lack_page
    add esp, 4
    pop edx
    pop ecx
    pop eax

    pop gs
    pop fs
    pop es
    pop ds
    add esp, 4
    iret
