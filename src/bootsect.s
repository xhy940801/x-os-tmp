BOOTSEG equ 0x07c0          ;boot at 0x7c000
INITSEG equ 0x9000          ;move bootseg to 0x90000
SETUPSEG equ 0x9020         ;setup seg at 0x90200
SETUPLEN equ 0x04           ;setup has 4 page
ROOT_DEV_POS equ 509        ;save 

SYSSIZE equ 0x4000          ;system max size is 0x30000
SYSSEG equ 0x1000           ;system load at 0x10000
ENDREAD equ SYSSIZE + SYSSEG

;把自身移动到0x9000处
start:
    mov ax, BOOTSEG
    mov ds, ax
    mov ax, INITSEG
    mov es, ax
    mov cx, 0x200
    mov si, 0
    mov di, 0
    rep movsb
    jmp INITSEG:nbseg

nbseg:
    mov ax, cs
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0xff00          ;设置堆栈指针

;通过int 0x13 载入setup程序
load_setup:
    mov ah, 0x02
    mov al, SETUPLEN
    mov ch, 0x00
    mov cl, 0x02
    mov dx, 0x0080
    mov bx, 0x0200
    int 0x13
    jnc load_success
    mov dx, 0x0080
    mov ax, 0x0000
    int 0x13
    jmp load_setup

load_success:
    mov cx, 24
    mov bx, 0x0007
    mov bp, loading_msg
    mov ax, 0x1301
    int 0x10

    mov ax, 0x0301
    mov [ROOT_DEV_POS], ax
;获取硬盘参数
    mov dl, 0x80
    mov al, 0x00
    mov ah, 0x08
    int 0x13
    mov al, ch
    mov ah, 0x00
    mov [max_track], ax
    mov ch, 0x00
    mov [sectors], cx
    mov ax, SYSSEG
    mov es, ax
    mov bx, 0x0000

    call readsys

    jmp SETUPSEG:0x0000
loading_msg:
    db "Loading system ..."
sectors:                    ;每磁道扇区数
    dw 0
sread:                      ;当前磁道已读取的扇区数
    dw 1 + SETUPLEN
head:                       ;当前磁头
    dw 0
track:                      ;当前磁道
    dw 0
max_track:                  ;每盘面最大磁道数
    dw 0
readsys:
    mov ax, es
    cmp ax, ENDREAD
    jb readit
    ret
readit:
    mov ax, [sectors]
    sub ax, [sread]
    mov cx, ax
    shl cx, 9
    add cx, bx
    jnc ok_read
    je ok_read

    mov ax, 0
    sub ax, bx
    shr ax, 9
ok_read:
    call read_track
    mov cx, ax
    add ax, [sread]
    cmp ax, [sectors]
    jnz ok_read2
    push ax
    mov ax, [max_track]
    cmp ax, [track]
    jz die
    mov ax, [head]
    cmp ax, 0x0001
    jnz ok_read3
    inc word [track]
ok_read3:
    mov ax, 0x0001
    xor [head], ax
    pop ax
    mov word [sread], 0
    jmp readsys
ok_read2:
    mov [sread], ax
    jmp readsys
die:
    jmp die
;读取软盘，并改变es:bx
read_track:
    push ax
    push bx
    push cx
    push dx
    mov dx, [track]
    mov cx, [sread]
    inc cx
    mov ch, dl
    mov dx, [head]
    mov dh, dl
    mov dl, 0x80
    mov ah, 0x02
    int 0x13
    cmp ah, 0x00
    jnz die
    pop dx
    pop cx
    pop bx
    pop ax
    push ax
    shl ax, 9
    add bx, ax
    jc cg_es
    pop ax
    ret
cg_es:
    mov ax, es
    add ax, 0x1000
    mov es, ax
    pop ax
    ret
fill:
    times 510-($-$$) db 0
    dw 0xaa55
