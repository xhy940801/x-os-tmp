#include "sched.h"

#include "asm.h"
#include "stddef.h"
#include "string.h"
#include "pcinfo_rb_tree.h"
#include "panic.h"
#include "tty0.h"

#define LEVEL_SIZE 16

char system_stack[4096];

struct tss_struct_t cpu0tss;
struct process_info_t process1info;
struct process_info_t* cur_process;
struct rb_tree_head_t pc_rb_tree_head;

long jiffies;

static struct sched_level_desc_t sched_levels1[LEVEL_SIZE];
static struct sched_level_desc_t sched_levels2[LEVEL_SIZE];

static struct sched_level_desc_t* active_levels;
static struct sched_level_desc_t* expire_levels;
static uint32_t active_count;
static uint32_t expire_count;

static inline uint16_t flush_rest_time(struct process_info_t* proc)
{
    return proc->nice + 1;
}

void scheduleto(struct cpu_stat_t* dst, struct cpu_stat_t* src);

void mktssdesc(unsigned int id, struct tss_struct_t* tss)
{
    struct gdt_descriptor_t* pdt = _gdt + id;
    pdt->l_limit = (short) (sizeof(struct tss_struct_t));
    pdt->l_base = (short) (((unsigned int) tss) & 0xffff);
    pdt->m_base = (char) ((((unsigned int) tss) >> 16) & 0xff);
    pdt->attr = 0x89 | ((sizeof(struct tss_struct_t) >> 8) & 0x0f00);
    pdt->h_base = (char) ((((unsigned int) tss) >> 24) & 0xff);
}

void turn_to_process1()
{
    _memset(&cpu0tss, 0, sizeof(cpu0tss));
    _memset(&process1info, 0, sizeof(process1info));
    cpu0tss.ss0 = 0x10;

    process1info.nice = 0;
    process1info.original_nice = 3;
    process1info.state = PROCESS_WAITING;
    process1info.pid = 1;
    process1info.parent = NULL;
    process1info.brother = NULL;
    process1info.owner_id = 0;
    process1info.group_id = 0;
    process1info.cpu_state.esp = (uint32_t) (system_stack + 4096);
    process1info.cpu_state.catalog_table_p = 0x00200000;
    process1info.catalog_table_v = (void*) 0xc0000000;
    
    open_tty0(VFS_FDAUTH_WRITE, &(process1info.fd_info.fds[1]));
    open_tty0(VFS_FDAUTH_WRITE, &(process1info.fd_info.fds[2]));

    rb_tree_init(&pc_rb_tree_head, &(process1info.rb_node));

    mktssdesc(3, &cpu0tss);
    _ltr(3 * 8);
    cur_process = &process1info;
}

void schedule_module_init()
{
    for(size_t i = 0; i < LEVEL_SIZE; ++i)
    {
        circular_list_init(&(sched_levels1[i].head));
        sched_levels1[i].count = 0;

        circular_list_init(&(sched_levels2[i].head));
        sched_levels2[i].count = 0;
    }
    active_levels = sched_levels1;
    expire_levels = sched_levels2;

    active_count = 0;
    expire_count = 0;
}

void schedule()
{
    kassert(cur_process->nice < LEVEL_SIZE);

    --cur_process->rest_time;
    if(cur_process->state == PROCESS_RUNNING)
    {
        circular_list_remove(&(cur_process->sched_node));
        if(cur_process->rest_time == 0)
        {
            cur_process->rest_time = flush_rest_time(cur_process);
            circular_list_insert(&(expire_levels[cur_process->nice].head), &(cur_process->sched_node));
            --active_count;
            ++expire_count;
        }
        else
            circular_list_insert(active_levels[cur_process->nice].head.pre, &(cur_process->sched_node));
    }

    if(active_count == 0)
    {
        struct sched_level_desc_t* t_levels = active_levels;
        active_levels = expire_levels;
        expire_levels = t_levels;

        uint32_t t_count = active_count;
        active_count = expire_count;
        expire_count = t_count;
    }
    
    struct process_info_t* new_process = &process1info;
    for(size_t i = LEVEL_SIZE - 1; i < LEVEL_SIZE; --i)
    {
        if(active_levels[i].count == 0)
            continue;
        kassert(active_levels[i].head.next != &(active_levels[i].head));
        new_process = parentof(active_levels[i].head.next, struct process_info_t, sched_node);
    }

    if(new_process == cur_process)
        return;

    struct process_info_t* old_process = cur_process;
    cur_process = new_process;
    scheduleto(&(new_process->cpu_state), &(old_process->cpu_state));//这后面加任何东西都可能出错
}

void in_sched_queue(struct process_info_t* proc)
{
     struct sched_level_desc_t* t_levels;
     if(proc->rest_time > 0)
         t_levels = active_levels;
     else
     {
         t_levels = expire_levels;
         proc->rest_time = flush_rest_time(proc);
     }
     proc->state = PROCESS_RUNNING;
     circular_list_insert(t_levels[proc->nice].head.pre, &(proc->sched_node));
}

void out_sched_queue(struct process_info_t* proc)
{
    kassert(proc->state == PROCESS_RUNNING);
    circular_list_remove(&(proc->sched_node));
}
