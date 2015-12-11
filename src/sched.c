#include "sched.h"

#include "asm.h"
#include "stddef.h"
#include "string.h"
#include "pcinfo_rb_tree.h"
#include "panic.h"
#include "tty0.h"
#include "vfs.h"
#include "mem.h"
#include "printk.h"
#include "interrupt.h"

union process_sys_page_t process1page;

struct tss_struct_t cpu0tss;

struct process_info_t* cur_process;
struct rb_tree_head_t pc_rb_tree_head;

long jiffies;

static struct sched_queue_desc_t sched_queue1;
static struct sched_queue_desc_t sched_queue2;

static struct sched_queue_desc_t* active_queue;
static struct sched_queue_desc_t* expire_queue;

static inline uint16_t flush_rest_time(struct process_info_t* proc)
{
    return proc->nice + 1;
}

void scheduleto(struct cpu_stat_t* dst, struct cpu_stat_t* src);

int schedulecpy(struct cpu_stat_t* src, union process_sys_page_t* dst_start, union process_sys_page_t* src_start);

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
    _memset(&process1page.process_info, 0, sizeof(process1page.process_info));
    cpu0tss.ss0 = 0x10;
    cpu0tss.esp0 = ((uint32_t) process1page.stack) + 8192;

    process1page.process_info.nice = 0;
    process1page.process_info.original_nice = 3;
    process1page.process_info.state = PROCESS_RUNNING;
    process1page.process_info.pid = 1;
    process1page.process_info.parent = NULL;
    process1page.process_info.brother = NULL;
    process1page.process_info.owner_id = 0;
    process1page.process_info.group_id = 0;
    process1page.process_info.cpu_state.esp = (uint32_t) (process1page.stack + 8192);
    process1page.process_info.cpu_state.catalog_table_p = 0x00200000;
    process1page.process_info.catalog_table_v = (uint32_t*) 0xc0100000;
    process1page.process_info.rest_time = 5;
    
    init_fd_info(&process1page.process_info.fd_info);
    vfs_bind_fd(1, VFS_FDAUTH_WRITE, get_tty0_inode(), &process1page.process_info.fd_info);
    vfs_bind_fd(2, VFS_FDAUTH_WRITE, get_tty0_inode(), &process1page.process_info.fd_info);
    process1page.process_info.fd_info.fd_size = 3;

    rb_tree_init(&pc_rb_tree_head, &(process1page.process_info.rb_node));

    mktssdesc(3, &cpu0tss);
    _ltr(3 * 8);
    cur_process = &process1page.process_info;
    process1page.process_info.catalog_table_v = (uint32_t*) 0xc0100000;
}

void init_schedule_module()
{
    for(size_t i = 0; i < SCHED_LEVEL_SIZE; ++i)
    {
        circular_list_init(&(sched_queue1.levels[i].head));

        circular_list_init(&(sched_queue2.levels[i].head));
    }
    active_queue = &sched_queue1;
    expire_queue = &sched_queue2;

    active_queue->count = 0;
    expire_queue->count = 0;
    in_sched_queue(cur_process);
}

void schedule()
{
    kassert(cur_process->nice < SCHED_LEVEL_SIZE);

    --cur_process->rest_time;
    if(cur_process->state == PROCESS_RUNNING)
    {
        circular_list_remove(&(cur_process->sched_node));
        if(cur_process->rest_time == 0)
        {
            cur_process->rest_time = flush_rest_time(cur_process);
            circular_list_insert(expire_queue->levels[cur_process->nice].head.pre, &(cur_process->sched_node));
            --active_queue->count;
            ++expire_queue->count;
            cur_process->sched_queue = expire_queue;
        }
        else
            circular_list_insert(active_queue->levels[cur_process->nice].head.pre, &(cur_process->sched_node));
    }
    if(active_queue->count == 0)
    {
        //printk("change\n");
        struct sched_queue_desc_t* t_queue = active_queue;
        active_queue = expire_queue;
        expire_queue = t_queue;
    }
    
    struct process_info_t* new_process = &process1page.process_info;
    for(size_t i = SCHED_LEVEL_SIZE - 1; i < SCHED_LEVEL_SIZE; --i)
    {
        if(circular_list_is_empty(&active_queue->levels[i].head))
            continue;
        /*printk(
            "\x1b\x0c""[%u] [%u] [%u] [%u]""\x1b\x07",
            active_queue->count,
            circular_list_size(&sched_queue1.levels[i].head),
            expire_queue->count,
            circular_list_size(&sched_queue2.levels[i].head)
        );*/
        new_process = parentof(active_queue->levels[i].head.next, struct process_info_t, sched_node);
    }

    //printk("from pid [%d] to pid [%d]\n", cur_process->pid, new_process->pid);
    if(new_process == cur_process)
        return;

    struct process_info_t* old_process = cur_process;
    cur_process = new_process;
    union process_sys_page_t* new_process_page = parentof(new_process, union process_sys_page_t, process_info);
    cpu0tss.esp0 = (uint32_t) (new_process_page->stack + 8192);
    scheduleto(
        &(new_process->cpu_state),
        &(old_process->cpu_state)
    );//这后面加任何东西都可能出错
}

void in_sched_queue(struct process_info_t* proc)
{
    kassert(active_queue != NULL);
    kassert(expire_queue != NULL);
    if(proc->rest_time > 0)
        proc->sched_queue = active_queue;
    else
    {
        proc->sched_queue = expire_queue;
        proc->rest_time = flush_rest_time(proc);
    }
    proc->state = PROCESS_RUNNING;
    circular_list_insert(proc->sched_queue->levels[proc->nice].head.pre, &(proc->sched_node));
    ++proc->sched_queue->count;
}

void out_sched_queue(struct process_info_t* proc)
{
    kassert(proc->state == PROCESS_RUNNING);
    circular_list_remove(&(proc->sched_node));
    --proc->sched_queue->count;
}

int pid_alloc()
{
    static int _last_pid = 100;
    return ++_last_pid;
}

int sys_fork()
{
    union process_sys_page_t* new_process = (union process_sys_page_t*) get_pages(1, 1, MEM_FLAGS_P | MEM_FLAGS_K);
    int ret = 3;
    __asm__ volatile(
        "push %3\n"
        "push %2\n"
        "push %1\n"
        "call schedulecpy\n"
        "add $12, %%esp\n"
        "mov %%eax, %0"
        :"=g"(ret)
        :"g"(&cur_process->cpu_state),"g"(new_process),"g"(parentof(cur_process, union process_sys_page_t, process_info)->stack)
        :"memory","eax","edi","esi"
    );
    if(ret == 0)
        return 0;
    kassert(ret == 1);
    uint32_t stack_end = (uint32_t) parentof(cur_process, union process_sys_page_t, process_info)->stack;
    new_process->process_info.cpu_state.esp = ((uint32_t) new_process->stack) + (cur_process->cpu_state.esp - stack_end) - 4;
    
    new_process->process_info.pid = pid_alloc();
    new_process->process_info.parent = cur_process;
    new_process->process_info.brother = cur_process->son;
    cur_process->son = &new_process->process_info;
    new_process->process_info.son = NULL;
    
    new_process->process_info.rest_time = cur_process->rest_time - (cur_process->rest_time >> 1);
    cur_process->rest_time >>= 1;
    if(cur_process->rest_time == 0)
        cur_process->rest_time = 1;

    if(mem_fork(&new_process->process_info, cur_process) < 0)
    {
        free_pages(new_process, 1);
        return -1;
    }
    if(fd_fork(&new_process->process_info, cur_process) < 0)
    {
        free_pages(new_process, 1);
        return -1;
    }

    in_sched_queue(&new_process->process_info);
    schedule();
    return new_process->process_info.pid;
}

void do_timer()
{
    const size_t PROC_LEN = 32;
    ++jiffies;
    struct process_info_t* procs[PROC_LEN];
    while(1)
    {
        size_t len = ready_processes(procs, PROC_LEN);
        for(size_t i = 0; i < len; ++i)
            in_sched_queue(procs[i]);
        if(len < PROC_LEN)
            break;
    }
    _outb(0x20, 0x20);
    schedule();
}

void on_timer_interrupt();

void init_auto_schedule_module()
{
    setup_intr_desc(0x20, on_timer_interrupt, 0);

    const size_t hz = 1000;
    const size_t thz = 1193180;
    const size_t od = thz / hz;
    _outb(0x43, 0x36);
    _outb(0x40, od & 0xff);
    _outb(0x40, (od >> 8) & 0xff);

    clear_8259_mask(0);
}

void sys_yield()
{
    schedule();
}
