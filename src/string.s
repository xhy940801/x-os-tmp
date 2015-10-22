global _memcpy
global _memset
global _memmove

_memcpy:
    push edi
    push esi
    mov edi, [esp + 12]
    mov esi, [esp + 16]
    mov ecx, [esp + 20]
    shr ecx, 2
    rep movsd
    mov ecx, [esp + 20]
    and ecx, 0x04
    rep stosb
    pop esi
    pop edi
    ret

_memset:
    push edi
    mov edi, [esp + 8]
    mov eax, [esp + 12]
    mov ecx, [esp + 16]
    shr ecx, 2
    rep stosd
    mov ecx, [esp + 16]
    and ecx, 0x04
    rep stosb
    pop edi
    ret

_memmove:
    push edi
    push esi
    mov edi, [esp + 12]
    mov esi, [esp + 16]
    mov ecx, [esp + 20]
    cmp esi, edi
    jb  _memrightmove
_memleftmove:
    shr ecx, 2
    rep movsd
    mov ecx, [esp + 20]
    and ecx, 0x04
    rep stosb
    pop esi
    pop edi
    ret
_memrightmove:
    std
    add edi, ecx
    add esi, ecx
    shr ecx, 2
    rep movsd
    mov ecx, [esp + 20]
    and ecx, 0x04
    rep movsb
    cld
    pop esi
    pop edi
    ret
