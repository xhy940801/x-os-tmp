BOOTSEG equ 0x9000          ;引导程序位置
SYSSEG equ 0x1000           ;system位置
SETUPSEG equ 0x9020         ;setup程序位置
SYSMAXLEN equ 0x8000        ;system最大 0x80000 byte

init:
    mov ax, BOOTSEG
    mov ds, ax

get_memory_size:
    mov ax, 0xe801
    int 0x15
    mov [ds:0x00], ax
    mov [ds:0x02], bx

get_video_model:
    mov ah, 0x0f
    int 0x10
    mov [ds:0x04], bx
    mov [ds:0x06], ax

get_video_other_info:
    mov ah, 0x12
    mov bl, 0x10
    int 0x10
    mov [ds:0x0a], bx
    mov [ds:0x0c], cx

get_hd0_info:
    mov ax, 0x0000
    mov ds, ax
    lds si, [ds:4*0x41]
    mov ax, BOOTSEG
    mov es, ax
    mov di, 0x0080
    mov cx, 0x10
    std
    rep movsb

get_hd1_info:
    mov ax, 0x0000
    mov ds, ax
    lds si, [ds:4*0x46]
    mov ax, BOOTSEG
    mov es, ax
    mov di, 0x0090
    mov cx, 0x10
    std
    rep movsb

check_hd1:
    mov ah, 0x15
    mov al, 0x00
    mov dl, 0x81
    int 0x13
    cmp ah, 0x03
    jz  mov_sysseg

hasnot_hd1:
    mov ax, BOOTSEG
    mov es, ax
    mov di, 0x0090
    mov cx, 0x10
    mov ax, 0x0000
    std
    rep stosb

mov_sysseg:
    cli
    cld
    mov ax, 0x0000
    mov es, ax
    mov di, 0x0000
    mov ax, SYSSEG
    mov ds, ax
    mov si, 0x0000
    mov cx, 0x8000
mov_sysseg_loop:
    mov ax, es
    cmp ax, SYSSEG + SYSMAXLEN
    jz  end_mov
    mov cx, 0x8000
    rep movsw
    mov ax, es
    add ax, 0x1000
    mov es, ax
    mov ax, ds
    add ax, 0x1000
    mov ds, ax
    mov si, 0x0000
    mov di, 0x0000
    jmp mov_sysseg_loop

end_mov:
    mov ax, SETUPSEG
    mov ds, ax
    mov es, ax
    lidt [es:idt_opt]
    lgdt [es:gdt_opt]

init_a20:
    in  al, 0x92
    or  al, 00000010b
    out 0x92, al

init_8259a:
    mov al, 0x11
    out 0x20, al
    dw 0x00eb, 0x00eb
    out 0xa0, al
    dw 0x00eb, 0x00eb

    mov al, 0x20
    out 0x21, al
    dw 0x00eb, 0x00eb

    mov al, 0x28
    out 0xa1, al
    dw 0x00eb, 0x00eb

    mov al, 0x04
    out 0x21, al
    dw 0x00eb, 0x00eb

    mov al, 0x02
    out 0xa1, al
    dw 0x00eb, 0x00eb

    mov al, 0x01
    out 0x21, al
    dw 0x00eb, 0x00eb
    out 0xa1, al
    dw 0x00eb, 0x00eb

    mov al, 0xff
    out 0x21, al
    dw 0x00eb, 0x00eb
    out 0xa1, al

goto_protected_mode:
    mov ax, 0x0001
    lmsw ax
    sti
    jmp dword 0x08:0x00

gdt:
    dq  0x0000000000000000
    dq  0x00cf9a000000ffff
    dq  0x00cf92000000ffff
end_gdt:

idt_opt:
    dw  0x0000
    dd  SETUPSEG * 0x10 + 0x00000000

gdt_opt:
    dw end_gdt - gdt
    dd SETUPSEG * 0x10 + gdt
