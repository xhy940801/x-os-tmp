#pragma once

void printk(const char* fmt, ...);

#define _print_to_eax(x) __asm__ volatile("mov %0, %%eax"::"m"(x):"%eax")