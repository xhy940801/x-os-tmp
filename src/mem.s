global on_lack_of_page

extern process_lack_page

on_lack_of_page:
    push eax
    push ecx
    push edx
    mov eax, [esp + 12]
    push eax
    cld
    call process_lack_page
    add esp, 4
    pop edx
    pop ecx
    pop eax
    iret
