#include "interrupt.h"

#include "asm.h"

struct idt_desc_t
{
    uint16_t l_base;
    uint16_t selector;
    uint16_t type;
    uint16_t h_base;
};

extern struct idt_desc_t _idt[];

void setup_intr_desc(size_t id, void* callback, uint16_t dpl)
{
    _idt[id].l_base = (uint16_t) (((uint32_t) callback) & 0x0000ffff);
    _idt[id].selector = 0x08;
    _idt[id].type = 0x8e00 | (dpl << 13);
    _idt[id].h_base = (uint16_t) (((uint32_t) callback) >> 16);
}

void setup_trap_desc(size_t id, void* callback, uint16_t dpl)
{
    _idt[id].l_base = (uint16_t) (((uint32_t) callback) & 0x0000ffff);
    _idt[id].selector = 0x08;
    _idt[id].type = 0x8f00 | (dpl << 13);
    _idt[id].h_base = (uint16_t) (((uint32_t) callback) >> 16);
}

void set_8259_mask(uint8_t port)
{
    int p = 0x21;
    if(port >= 8)
    {
        p = 0xa1;
        port -= 8;
    }
    uint32_t mask = _inb(p);
    _outb(p, mask | (1 << port));
}

void clear_8259_mask(uint8_t port)
{
    int p = 0x21;
    if(port >= 8)
    {
        p = 0xa1;
        port -= 8;
    }
    uint32_t mask = _inb(p);
    _outb(p, mask & (~(1 << port)));

}
