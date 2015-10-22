extern main1
extern system_stack

global _idt
global _gdt
global startup_32
global ignore_idt
[BITS 32]
startup_32:
setregs:
    mov eax, 0x10
    mov ds, eax
    mov es, eax
    mov fs, eax
    mov gs, eax

    mov ss, eax
    mov esp, system_stack - 0xc0000000 + 4096

    call setup_idt
    call setup_gdt
    mov eax, 0x10
    mov ds, eax
    mov es, eax
    mov fs, eax
    mov gs, eax
    jmp dword 0x08:__resetcsreg - 0xc0000000
__resetcsreg:
    mov ss, eax
    mov esp, system_stack - 0xc0000000 + 4096

    mov eax, 0x0000
checka20:
    inc eax
    mov eax, [0x000000]
    cmp eax, [0x100000]
    je checka20
    
check287or387:
    mov eax, cr0
    and eax, 0x80000011
    or  eax, 0x02
    
    fninit
    fstsw ax
    cmp ax, 0
    je  check287
    mov eax, cr0
    xor eax, 0x06
    mov cr0, eax

    jmp after_page_tables

check287:
    db  0xdb, 0xe4
    jmp after_page_tables

setup_idt:
    mov eax, _idt - 0xc0000000
    mov edi, eax
    mov eax, ignore_idt - 0xc0000000
    and eax, 0xffff
    or  eax, 0x00080000
    mov edx, ignore_idt - 0xc0000000
    mov dx, 0x8e00
    mov ecx, 0xff
    mov eax, 0
    mov edx, 0
rp_setidt:
    mov [edi + 0], eax
    mov [edi + 4], edx
    add edi, 0x08
    dec ecx
    jne rp_setidt
    lidt [idt_descr - 0xc0000000]
    ret

setup_gdt:
    lgdt [gdt_descr - 0xc0000000]
    ret

    times 0x6000-($-$$) db 0

after_page_tables:
    mov edi, 0x5000
    call _clean_page_table
    mov dword [0x0000], 0x00001007
    mov dword [0x0004], 0x00002007
    mov dword [0x0008], 0x00003007
    mov dword [0x000c], 0x00004007
    mov dword [0x0c00], 0x00001007
    mov dword [0x0c04], 0x00002007
    mov dword [0x0c08], 0x00003007
    mov dword [0x0c0c], 0x00004007
    mov dword [0x0ffc], 0x00005007
    mov edi, 0x5000
    mov eax, 0x1000000 + 0x007

fill_page_table:
    sub edi, 0x04
    sub eax, 0x1000
    mov [edi], eax
    cmp edi, 0x1000
    jne fill_page_table
    
    mov eax, 0x00
    mov cr3, eax
    mov eax, cr0
    or  eax, 0x80000000
    mov cr0, eax
    
    push 0x00
    push 0x00
    mov word [0x300000], 1
    mov esp, system_stack + 4096
    call main1 + 0xc0000000
    jmp $

_clean_page_table:
    sub edi, 4,
    mov dword [edi], 0
    jne _clean_page_table
    ret
    
ignore_idt:
    iret

idt_descr:
    dw 256 * 8 - 1
    dd _idt - 0xc0000000

gdt_descr:
    dw 256 * 8 - 1
    dd _gdt - 0xc0000000

hahastr:
    db 0x00

align 8
_idt:
    times 256 dq 0

align 8
_gdt:
    dq 0x0000000000000000
    dq 0x00cf9a000000ffff
    dq 0x00cf92000000ffff
    dq 0x0000000000000000
    dq 0x00cb9a000000ffff
    dq 0x00cb92000000ffff
    times 250 dq 0
__boot_end:
