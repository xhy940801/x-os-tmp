#pragma once

void _sti();
void _cli();
void _std();
void _cld();
void _outb(int port, int data);
void _outw(int port, int data);
int _inb(int port);
int _inw(int port);
void _ltr(int index);
void _lcr3(unsigned long pos);