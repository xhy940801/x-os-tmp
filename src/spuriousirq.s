global on_spurious_irq

extern print_spurious_irq

on_spurious_irq
    iret
    push ds
    push es
    push fs
    push gs

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
    pop edx
    pop ecx
    pop eax

    pop gs
    pop fs
    pop es
    pop ds
    iret
