#include "printk.h"
#include "panic.h"
#include "stdint.h"
#include "asm.h"

void print_spurious_irq()
{
    static int c = 0;
    _outb(0x20, 0x0b);
    uint32_t irr = _inb(0x20);
    if(irr & 0x80)
        panic("uncatch irq7");
    else
        printk("fucking spurious count [%d]\n", ++c);
}

