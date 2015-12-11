global fucking_irq7

extern print_fuck_irq7

fucking_irq7
    push eax
    push ecx
    push edx
    cld
    cli
    mov eax, 0x10
    mov ds, eax
    mov es, eax
    mov fs, eax
    mov gs, eax
    call print_fuck_irq7
    mov eax, 0x2b
    mov ds, eax
    mov es, eax
    mov fs, eax
    mov gs, eax
    pop edx
    pop ecx
    pop eax
    iret
