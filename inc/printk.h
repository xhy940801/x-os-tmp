#pragma once

void printk(const char* fmt, ...);

#define _print_to_eax(x) __asm__ volatile("mov %0, %%eax"::"m"(x):"%eax")

#define P_LRED      "\x1b\x0c"
#define P_LGREEN    "\x1b\x0a"
#define P_LBLUE     "\x1b\x09"
#define P_LWHITE    "\x1b\x0f"
#define P_RED       "\x1b\x04"
#define P_GREEN     "\x1b\x02"
#define P_BLUE      "\x1b\x01"
#define P_WHITE     "\x1b\x07"