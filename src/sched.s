global scheduleto
global iret_to_user_level
global schedulecpy
global on_timer_interrupt

extern do_timer
extern get_locker_count
extern set_locker_count

scheduleto:
    mov eax, [esp + 8]
    fnsave [eax + 8]
    mov [eax], esp

    mov eax, [esp + 4]
    frstor [eax + 8]
    mov edx, [eax + 4]
    mov cr3, edx
    mov esp, [eax]
    ret

schedulecpy:
    mov eax, [esp + 4]
    fnsave [eax + 8]
    mov [eax], esp
    mov dword [esp - 4], _ret_zero
    mov edi, [esp + 8]
    mov esi, [esp + 12]
    mov ecx, 2048
    rep movsd
    mov eax, 1
    ret

_ret_zero:
    mov eax, 0
    ret

iret_to_user_level:
	mov eax, [esp + 4]
	sub eax, 0xc0000000
    push 0x2b
    push 0xb1000000
	push 0x200
	push 0x23
	push eax
    mov eax, 0x2b
    mov ds, eax
    mov es, eax
    mov fs, eax
    mov gs, eax
	iret

on_timer_interrupt:
    push ds
    push es
    push fs
    push gs

    sub esp, 4
    push eax
    push ecx
    push edx
    cld
    
    mov eax, 0x10
    mov ds, eax
    mov es, eax
    mov fs, eax
    mov gs, eax

    call get_locker_count
    mov [esp + 12], eax
    push 0
    call set_locker_count
    add esp, 4

    call do_timer

    mov eax, [esp + 12]
    push eax
    call set_locker_count
    add esp, 4

    pop edx
    pop ecx
    pop eax
    add esp, 4

    pop gs
    pop fs
    pop es
    pop ds
    iret
