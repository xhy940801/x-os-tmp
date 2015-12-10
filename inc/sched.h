#pragma once

#include "stdint.h"
#include "stddef.h"

#include "vfs.h"
#include "rb_tree.h"
#include "wait.h"
#include "ksemaphore.h"

#pragma pack(1)

struct gdt_descriptor_t
{
    short l_limit;
    short l_base;
    char m_base;
    short attr;
    char h_base;
};

struct tss_struct_t
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
};
#pragma pack()

struct cpu_stat_t
{
    uint32_t esp;
    uint32_t catalog_table_p;
    char fpu_state[108];
};

struct waitlist_node_desc_t
{
    struct list_node_t node;
    union
    {
        int waiting;
        size_t demand;
    };
};

struct process_info_t
{
    //rb-tree
    struct rb_tree_node_t rb_node;
    //sched
    uint8_t nice;
    uint8_t original_nice;
    uint16_t state;
    struct list_node_t sched_node;
    //process-info
    int pid;
    struct process_info_t* parent;
    struct process_info_t* brother;
    struct process_info_t* son;
    int last_errno;
    int sub_errno;
    uint32_t owner_id;
    uint32_t group_id;
    uint32_t sign;
    //for-wait
    struct sleep_desc_t sleep_info;
    struct waitlist_node_desc_t waitlist_node;
    //sched
    uint16_t rest_time;
    uint16_t _padding;
    //fd-info
    struct fd_info_t fd_info;
    //cpu state
    uint32_t* catalog_table_v;
    struct cpu_stat_t cpu_state;
};

union process_sys_page_t
{
    char stack[8192];
    struct process_info_t process_info;
};

struct sched_level_desc_t
{
    struct list_node_t head;
    size_t count;
};

enum
{
    PROCESS_RUNNING = 1,
    PROCESS_WAITING = 2
};

void turn_to_process1();

void schedule_module_init();
void schedule();

void in_sched_queue(struct process_info_t* proc);
void out_sched_queue(struct process_info_t* proc);

void iret_to_user_level(void* target);

int sys_fork();

extern struct gdt_descriptor_t _gdt[];
extern struct process_info_t* cur_process;
extern long jiffies;
