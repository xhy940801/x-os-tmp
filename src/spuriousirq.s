global on_spurious_irq

extern print_spurious_irq

on_spurious_irq
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
    call print_spurious_irq
    mov eax, 0x2b
    mov ds, eax
    mov es, eax
    mov fs, eax
    mov gs, eax
    pop edx
    pop ecx
    pop eax
    iret
