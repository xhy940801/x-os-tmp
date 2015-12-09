global scheduleto
global iret_to_user_level

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

iret_to_user_level:
	mov eax, [esp + 4]
	sub eax, 0xc0000000
    push 0x28
    push 0xb1000000
	push 0x00
	push 0x20
	push eax
	iret