#include "sched.h"

#include "asm.h"
#include "stddef.h"
#include "string.h"

char system_stack[4096];

tss_struct_t cpu0tss;
process_info_t process1info;
process_info_t* cur_process;

void mktssdesc(unsigned int id, tss_struct_t* tss)
{
    gdt_descriptor_t* pdt = _gdt + id;
    pdt->l_limit = (short) (sizeof(tss_struct_t));
    pdt->l_base = (short) (((unsigned int) tss) & 0xffff);
    pdt->m_base = (char) ((((unsigned int) tss) >> 16) & 0xff);
    pdt->attr = 0x89 | ((sizeof(tss_struct_t) >> 8) & 0x0f00);
    pdt->h_base = (char) ((((unsigned int) tss) >> 24) & 0xff);
}

void turn_to_process1()
{
    _memset(&cpu0tss, 0, sizeof(cpu0tss));
    cpu0tss.ss0 = 0x10;

    process1info.nice = 0;
    process1info.original_nice = 3;
    process1info.state = PROCESS_READY;
    process1info.pid = 1;
    process1info.ppid = 0;
    process1info.wait_list = NULL;
    process1info.next = NULL;
    process1info.sys_stack = system_stack + 4096;
    process1info.page_catalog_table = 0x00000000;

    mktssdesc(3, &cpu0tss);
    _ltr(3 * 8);
    cur_process = &process1info;
}
