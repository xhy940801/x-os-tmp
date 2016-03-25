SYSPOS equ 0xc0100000

extern main1
extern process1page

global startup_32
global _idt
global _gdt
global ignore_idt
global _bios_sys_params
[BITS 32]
startup_32:
    mov eax, 0x10
    mov ds, eax
    mov es, eax
    mov fs, eax
    mov gs, eax

    mov edi, 0x200000
    mov esi, 0x0000
    mov ecx, 0x80000
    cld
    rep movsb

    jmp setregs + 0x200000
    times 0x1000-($-$$) db 0
setregs:
    mov eax, 0x10
    mov ds, eax
    mov es, eax
    mov fs, eax
    mov gs, eax



    mov ss, eax
    mov esp, process1page - SYSPOS + 0x200000 + 8192

    mov eax, [0x90000]
    mov [_bios_sys_params - SYSPOS + 0x200000], eax
    call setup_page_table

    mov eax, 0x00200000
    mov cr3, eax
    mov eax, cr0
    or  eax, 0x80000000
    mov cr0, eax

    jmp 0x08:_reset_pos
_reset_pos:
    mov esp, process1page + 8192

    call setup_idt
    call setup_gdt
    call check287or387

    mov esp, process1page + 8192
    cli
    call main1
    jmp $

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
    ret

check287:
    db  0xdb, 0xe4
    ret

setup_page_table:
    mov eax, 0
    mov edi, 0x100000
    mov ecx, 0x40000
    rep stosd

    mov edi, 0x200000
    mov ecx, 0x400
    rep stosd

    mov eax, 0x0000 + 0x007
    mov edi, 0x0000
    mov ecx, 0x1000
    call map_table

    mov eax, 0x100000 + 0x007
    mov edi, 0x100000
    mov ecx, 0x100000 + 0x4000
    call map_table

    mov eax, 0xa0000 + 0x007
    mov edi, 0x200000 - 0x0180
    mov ecx, 0x200000
    call map_table

    mov dword [0x200000], 0x00000007

    mov edi, 0x200c00
    mov eax, 0x00100007
loop_set_catalog:
    mov [edi], eax
    add edi, 0x04
    add eax, 0x1000
    cmp edi, 0x201000
    jne loop_set_catalog

    ret

setup_idt:
    mov eax, _idt
    mov edi, eax
    mov eax, ignore_idt
    and eax, 0xffff
    or  eax, 0x00080000
    mov edx, ignore_idt
    mov dx, 0x8e00
    mov ecx, 0xff
    ;方便调试不设置idt
    mov eax, 0
    mov edx, 0
rp_setidt:
    mov [edi + 0], eax
    mov [edi + 4], edx
    add edi, 0x08
    dec ecx
    jne rp_setidt
    lidt [idt_descr]
    ret

setup_gdt:
    lgdt [gdt_descr]
    ret

map_table:
    mov [edi], eax
    add eax, 0x1000
    add edi, 0x04
    cmp edi, ecx
    jne map_table
    ret

ignore_idt:
    iret

idt_descr:
    dw 256 * 8 - 1
    dd _idt

gdt_descr:
    dw 256 * 8 - 1
    dd _gdt

align 8
_idt:
    times 256 dq 0

align 8
_gdt:
    dq 0x0000000000000000
    dq 0x00cf9a000000ffff
    dq 0x00cf92000000ffff
    dq 0x0000000000000000
    dq 0x00cbfa000000ffff
    dq 0x00cbf2000000ffff
    times 250 dq 0
_bios_sys_params:
    dq 0
    dq 0
