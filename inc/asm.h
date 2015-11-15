#pragma once

#define _sti() __asm__("sti")
#define _cli() __asm__("cli")
#define _std() __asm__("std")
#define _cld() __asm__("cld")
#define _outb(port, data) __asm__("outb %%al, %%dx"::"d"(port),"a"(data))
#define _outw(port, data) __asm__("outw %%ax, %%dx"::"d"(port),"a"(data))
#define _inb(port) ({char _v; __asm__("inb %%dx, %%al":"=a"(_v):"d"(port); _v;})
#define _inw(port) ({short _v; __asm__("inb %%dx, %%ax":"=a"(_v):"d"(port); _v;})
#define _ltr(index) __asm__("ltr %%ax"::"a"(index))
#define _lcr3(pos) __asm__("mov %%cr3, %%eax"::"a"(pos))
#define _bsf(src) ({int _v; __asm__("bsf %1, %0":"=r"(_v):"r"(src)); _v;})
#define _bsr(src) ({int _v; __asm__("bsr %1, %0":"=r"(_v):"r"(src)); _v;})
