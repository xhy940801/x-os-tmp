global scheduleto

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
