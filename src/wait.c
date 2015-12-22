#include "wait.h"

#include "stdint.h"
#include "stddef.h"

#include "errno.h"

#include "sched.h"
#include "panic.h"

#define WHEEL_SIZE 512
#define WHEEL_HASH(time) ((time) & 511)

static struct time_wheel_desc_t time_wheel[WHEEL_SIZE];
static struct list_node_t wait_list_head;

void init_wait_module()
{
    for(size_t i = 0; i < WHEEL_SIZE; ++i)
        circular_list_init(&(time_wheel[i].head));
    circular_list_init(&wait_list_head);
}

void kuninterruptwait(struct process_info_t* proc)
{
    proc->state = PROCESS_UNINTERRUPTABLE;
    circular_list_insert(&wait_list_head, &(proc->sleep_info.node));
}

void kwait(struct process_info_t* proc)
{
    proc->state = PROCESS_INTERRUPTABLE;
    circular_list_insert(&wait_list_head, &(proc->sleep_info.node));
}

void kwakeup(struct process_info_t* proc)
{
    kassert(proc->state == PROCESS_INTERRUPTABLE || proc->state == PROCESS_UNINTERRUPTABLE);
    circular_list_remove(&(proc->sleep_info.node));
}

void ksleep(struct process_info_t* proc, uint32_t tick)
{
    proc->state = PROCESS_INTERRUPTABLE;
    proc->sleep_info.wakeup_jiffies = tick + jiffies;
    circular_list_insert(&(time_wheel[WHEEL_HASH(proc->sleep_info.wakeup_jiffies)].head), &(proc->sleep_info.node));
}

uint32_t ready_processes(struct process_info_t* procs[], uint32_t max)
{
    size_t i;
    struct list_node_t* node = time_wheel[WHEEL_HASH(jiffies)].head.next;
    for(i = 0; i < max; node = node->next)
    {
        if(node == &time_wheel[WHEEL_HASH(jiffies)].head)
            return i;
        struct process_info_t* proc = parentof(node, struct process_info_t, sleep_info.node);
        if(proc->sleep_info.wakeup_jiffies > jiffies)
            continue;
        kassert(proc->state == PROCESS_INTERRUPTABLE);
        proc->sub_errno = ETIMEDOUT;
        circular_list_remove(&(proc->sleep_info.node));
        procs[i] = proc;
        ++i;
    }
    return i;
}

int sys_tsleep(uint32_t timeout)
{
    out_sched_queue(cur_process);
    if(timeout == 0)
        kwait(cur_process);
    else
        ksleep(cur_process, timeout);
    schedule();
    return 0;
}
