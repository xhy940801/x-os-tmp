#pragma once

#include "stdint.h"

#pragma pack(1)

typedef struct
{
    short l_limit;
    short l_base;
    char m_base;
    short attr;
    char h_base;
} gdt_descriptor_t;

typedef struct
{
    uint32_t back_link;
    uint32_t esp0, ss0;
    uint32_t esp1, ss1;
    uint32_t esp2, ss2;
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax, ecx, edx, ebx;
    uint32_t esp, ebp, esi, edi;
    uint32_t es, cs, ss, ds, fs, gs;
    uint32_t idt;
    uint32_t debug;
} tss_struct_t;
#pragma pack()

typedef struct process_info
{
    uint8_t nice;
    uint8_t original_nice;
    uint16_t state;
    int pid;
    int ppid;
    struct process_info* wait_list;
    struct process_info* next;
    void* sys_stack;
    uint32_t esp;
    uint32_t page_catalog_table;
} process_info_t;

enum
{
    PROCESS_READY = 1,
    PROCESS_WAIT = 2
};

void turn_to_process1();

extern gdt_descriptor_t _gdt[];
extern process_info_t* cur_process;
