global scheduleto
global iret_to_user_level
global schedulecpy

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
    fnsave [eax + 4]
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
    push 0x28
    push 0xb1000000
	push 0x00
	push 0x20
	push eax
	iret
