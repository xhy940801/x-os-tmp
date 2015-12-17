#include "hd_driver.h"

#include "stddef.h"
#include "stdint.h"
#include "asm.h"
#include "panic.h"
#include "printk.h"
#include "string.h"
#include "block_driver.h"
#include "block_buffer.h"
#include "circular_list.h"
#include "task_locker.h"
#include "sched.h"
#include "errno.h"

#define HD_OP_CAPACITY 32

enum hd_operation_type
{
    HD_OPERATION_READ,
    HD_OPERATION_WRITE
};

enum hd_elevator_state
{
    HD_ELEVATOR_UP,
    HD_ELEVATOR_DOWN
};

struct hd_operation_desc_t
{
    struct list_node_t node;
    struct block_buffer_desc_t* start;
    size_t count;
    struct process_info_t* proc;
    enum hd_operation_type op_type;
};

struct hd_operation_append_desc_t
{
    struct block_buffer_desc_t* start;
    size_t count;
    int result;
    enum hd_operation_type op_type;
};

struct hd_elevator_desc_t
{
    struct hd_operation_desc_t hd_opertations[HD_OP_CAPACITY];
    enum hd_elevator_state state;
    struct list_node_t up_head;
    struct list_node_t down_head;
    struct list_node_t free_head;
    struct list_node_t read_wait_list_head;
    struct list_node_t write_wait_list_head;
    size_t write_priority;
};

struct hd_subdriver_desc_t
{
    int baseport;       //0 if not exist
    int slavebit;
    size_t startlba;
    size_t len;
};

struct hd_info_t
{
    int baseport;
    int slavebit;
    int flags;
    size_t len;
    struct hd_elevator_desc_t elevator;
};

struct lba48_partition_table_desc_t
{
    uint8_t flags;
    uint8_t signature1;
    uint16_t lbahi;
    uint8_t system_id;
    uint8_t signature2;
    uint16_t lenhi;
    uint32_t lbalo;
    uint32_t lenlo;
};

static struct hd_subdriver_desc_t hd_subdrivers[4];
static struct hd_info_t hd_infos[1];
static uint8_t driver_last_selectors[1];

#define PORT_DATA           0x0000
#define PORT_SELECTORCOUNT  0x0002
#define PORT_LBALO          0x0003
#define PORT_LBAMID         0x0004
#define PORT_LBAHI          0x0005
#define PORT_DRIVERSELECT   0x0006
#define PORT_CMDSTATUS      0x0007
#define PORT_DRIVER_CTL     0x0206

static inline void read256word(int port, uint16_t* p)
{
    __asm__ (
        "rep insw"
        ::"d"(port),"c"(256),"D"(p)
        :"memory"
    );
}

static inline void write256word(int port, const uint16_t* p)
{
    for(size_t i = 0; i < 256; ++i)
        __asm__ (
            "outw %%ax, %%dx"
            ::"d"(port),"a"(p[i])
        );
}

int hd_pio_readoneblock(struct hd_info_t* info, size_t lba, char* buf)
{
    _outb(info->baseport + PORT_DRIVERSELECT, 0xe0 | info->slavebit | ((lba >> 24) & 0x0f));
    _outb(info->baseport + PORT_SELECTORCOUNT, 1);
    _outb(info->baseport + PORT_LBALO, lba & 0xff);
    _outb(info->baseport + PORT_LBAMID, ((lba >> 8) & 0xff));
    _outb(info->baseport + PORT_LBAHI, ((lba >> 16) & 0xff));
    _outb(info->baseport + PORT_CMDSTATUS, 0x20);
    //400ns delay
    int status;
    _inb(info->baseport + PORT_CMDSTATUS);
    _inb(info->baseport + PORT_CMDSTATUS);
    _inb(info->baseport + PORT_CMDSTATUS);
    _inb(info->baseport + PORT_CMDSTATUS);
    while(1)
    {
        status = _inb(info->baseport + PORT_CMDSTATUS);
        if(status & 0x21)
            return -1;
        if((status & 0x80) == 0)
            break;
    }
    while((status & 0x08) == 0)
        status = _inb(info->baseport + PORT_CMDSTATUS);
    read256word(info->baseport + PORT_DATA, (uint16_t*) buf);
    return 0;
}

int hd_pio_writeoneblock(struct hd_info_t* info, size_t lba, const char* buf)
{
    _outb(info->baseport + PORT_DRIVERSELECT, 0xe0 | info->slavebit | ((lba >> 24) & 0x0f));
    _outb(info->baseport + PORT_SELECTORCOUNT, 1);
    _outb(info->baseport + PORT_LBALO, lba & 0xff);
    _outb(info->baseport + PORT_LBAMID, ((lba >> 8) & 0xff));
    _outb(info->baseport + PORT_LBAHI, ((lba >> 16) & 0xff));
    _outb(info->baseport + PORT_CMDSTATUS, 0x30);
    //400ns delay
    int status;
    _inb(info->baseport + PORT_CMDSTATUS);
    _inb(info->baseport + PORT_CMDSTATUS);
    _inb(info->baseport + PORT_CMDSTATUS);
    _inb(info->baseport + PORT_CMDSTATUS);
    while(1)
    {
        status = _inb(info->baseport + PORT_CMDSTATUS);
        if(status & 0x21)
            return -1;
        if((status & 0x80) == 0)
            break;
    }
    while((status & 0x08) == 0)
        status = _inb(info->baseport + PORT_CMDSTATUS);
    write256word(info->baseport + PORT_DATA, (uint16_t*) buf);
    return 0;
}

int load_hd_params(struct hd_info_t* info)
{
    _outb(info->baseport + PORT_DRIVERSELECT, 0xa0 | info->slavebit);
    _outb(info->baseport + PORT_SELECTORCOUNT, 0);
    _outb(info->baseport + PORT_LBALO, 0);
    _outb(info->baseport + PORT_LBAMID, 0);
    _outb(info->baseport + PORT_LBAHI, 0);
    _outb(info->baseport + PORT_CMDSTATUS, 0xec);
    while(1)
    {
        int status = _inb(info->baseport + PORT_CMDSTATUS);
        if(status == 0)
            return -1;
        if((status & 0x80) == 0)
            break;
    }
    if(_inb(info->baseport + PORT_LBAMID) || _inb(info->baseport + PORT_LBAHI))
        return -1;
    while(1)
    {
        int status = _inb(info->baseport + PORT_CMDSTATUS);
        if(status & 0x01)
            return -1;
        if(status & 0x08)
            break;
    }
    uint16_t datas[256];
    read256word(info->baseport + PORT_DATA, datas);
    info->len = *((uint32_t*) (datas + 60));
    if(info->len == 0)
        return -1;
    return 0;
}

int load_hd_partition_table(struct hd_info_t* info, struct hd_subdriver_desc_t* start)
{
    char buf[512];
    int ret = hd_pio_readoneblock(info, 0, buf);
    if(ret < 0)
        return ret;
    int pc = 0;
    for(size_t i = 0; i < 4; ++i)
    {
        struct lba48_partition_table_desc_t* table = (struct lba48_partition_table_desc_t*) (buf + 446 + i * 16);
        if(table->flags != 0x01 && table->flags != 0x81)
            continue;
        if(table->signature1 != 0x14 || table->signature2 != 0xeb)
            continue;
        if(table->lbahi != 0)
            continue;
        if(table->lenhi != 0)
            continue;
        start[i].baseport = info->baseport;
        start[i].slavebit = info->slavebit;
        start[i].startlba = table->lbalo;
        start[i].len = table->lenlo;
        ++pc;
    }
    return pc;
}

int reset_hd_partition_table(struct hd_info_t* info)
{
    kassert(sizeof(struct lba48_partition_table_desc_t) == 16);
    char buf[512];
    int ret = hd_pio_readoneblock(info, 0, buf);
    if(ret < 0)
        return ret;
    struct lba48_partition_table_desc_t* table1 = (struct lba48_partition_table_desc_t*) (buf + 446);
    table1->flags = 0x81;
    table1->signature1 = 0x14;
    table1->signature2 = 0xeb;
    table1->lbahi = 0;
    table1->lenhi = 0;
    table1->lbalo = 0;
    table1->lenlo = 2 * 1024 * 1024;

    struct lba48_partition_table_desc_t* table2 = (struct lba48_partition_table_desc_t*) (buf + 446 + 16);
    table2->flags = 0x01;
    table2->signature1 = 0x14;
    table2->signature2 = 0xeb;
    table2->lbahi = 0;
    table2->lenhi = 0;
    table2->lbalo = 2 * 1024 * 1024;
    table2->lenlo = 20 * 1024 * 1024 - 2 * 1024 * 1024;
    return hd_pio_writeoneblock(info, 0, buf);
}

int hd_irq_start_read(struct block_buffer_desc_t* start, size_t count)
{
    return 0;
}

int hd_irq_start_write(struct block_buffer_desc_t* start, size_t count)
{
    return 0;
}

int hd_blocks_op(struct hd_operation_append_desc_t* hd_op_append, int timeout)
{
    struct block_buffer_desc_t* start = hd_op_append->start;
    size_t count = hd_op_append->count;
    kassert(count < 32);
    kassert(hd_op_append->op_type == HD_OPERATION_WRITE || hd_op_append->op_type == HD_OPERATION_READ);
    count <<= 2;
    
    struct hd_info_t* hd_info = &hd_infos[start->sub_driver >> 2];
    lock_task();
    if(circular_list_is_empty(&hd_info->elevator.free_head))
    {
        if(timeout < 0)
        {
            cur_process->last_errno = ETIMEDOUT;
            unlock_task();
            return -1;
        }
        circular_list_insert(
            hd_info->elevator.read_wait_list_head.pre,
            &cur_process->waitlist_node.node
        );
        out_sched_queue(cur_process);
        if(timeout == 0)
            kwait(cur_process);
        else
            ksleep(cur_process, timeout);
    }
    else
    {
        struct hd_operation_desc_t* free_node = parentof(hd_info->elevator.free_head.next, struct hd_operation_desc_t, node);
        circular_list_remove(&free_node->node);
        free_node->start = start;
        free_node->count = count;
        free_node->proc = cur_process;
        if(hd_info->elevator.state == HD_ELEVATOR_UP)
        {
            if(circular_list_is_empty(&hd_info->elevator.up_head))
            {
                kassert(circular_list_is_empty(&hd_info->elevator.down_head));
                int res;
                if(hd_op_append->op_type == HD_OPERATION_WRITE)
                    res = hd_irq_start_write(start, count);
                else
                    res = hd_irq_start_read(start, count);
                if(res < 0)
                {
                    unlock_task();
                    return res;
                }
                circular_list_insert(&hd_info->elevator.up_head, &free_node->node);
                out_sched_queue(cur_process);
                kuninterruptwait(cur_process);
            }
            else
            {
                if(timeout < 0)
                {
                    cur_process->last_errno = ETIMEDOUT;
                    unlock_task();
                    return -1;
                }
                if(start->block_no >
                    parentof(
                        hd_info->elevator.up_head.next,
                        struct hd_operation_desc_t,
                        node
                    )->start->block_no
                )
                {
                    struct list_node_t* c_node = hd_info->elevator.up_head.next;
                    while(
                        c_node->next != &hd_info->elevator.up_head &&
                        start->block_no > parentof(
                            c_node->next,
                            struct hd_operation_desc_t,
                            node
                        )->start->block_no
                    )
                        c_node = c_node->next;
                    circular_list_insert(c_node->pre, &free_node->node);
                }
                else
                {
                    struct list_node_t* c_node = hd_info->elevator.down_head.next;
                    while(
                        c_node != &hd_info->elevator.down_head &&
                        start->block_no < parentof(
                            c_node,
                            struct hd_operation_desc_t,
                            node
                        )->start->block_no
                    )
                        c_node = c_node->next;
                    circular_list_insert(c_node->pre, &free_node->node);
                }
                out_sched_queue(cur_process);
                if(timeout == 0)
                    kwait(cur_process);
                else
                    ksleep(cur_process, timeout);
            }
        }
        else
        {
            if(circular_list_is_empty(&hd_info->elevator.down_head))
            {
                kassert(circular_list_is_empty(&hd_info->elevator.up_head));
                int res;
                if(hd_op_append->op_type == HD_OPERATION_WRITE)
                    res = hd_irq_start_write(start, count);
                else
                    res = hd_irq_start_read(start, count);
                if(res < 0)
                {
                    unlock_task();
                    return res;
                }
                circular_list_insert(&hd_info->elevator.down_head, &free_node->node);
                out_sched_queue(cur_process);
                kuninterruptwait(cur_process);
            }
            else
            {
                if(timeout < 0)
                {
                    cur_process->last_errno = ETIMEDOUT;
                    unlock_task();
                    return -1;
                }
                if(start->block_no <
                    parentof(
                        hd_info->elevator.down_head.next,
                        struct hd_operation_desc_t,
                        node
                    )->start->block_no
                )
                {
                    struct list_node_t* c_node = hd_info->elevator.down_head.next;
                    while(
                        c_node->next != &hd_info->elevator.down_head &&
                        start->block_no < parentof(
                            c_node->next,
                            struct hd_operation_desc_t,
                            node
                        )->start->block_no
                    )
                        c_node = c_node->next;
                    circular_list_insert(c_node->pre, &free_node->node);
                }
                else
                {
                    struct list_node_t* c_node = hd_info->elevator.up_head.next;
                    while(
                        c_node != &hd_info->elevator.up_head &&
                        start->block_no > parentof(
                            c_node,
                            struct hd_operation_desc_t,
                            node
                        )->start->block_no
                    )
                        c_node = c_node->next;
                    circular_list_insert(c_node->pre, &free_node->node);
                }
                out_sched_queue(cur_process);
                if(timeout == 0)
                    kwait(cur_process);
                else
                    ksleep(cur_process, timeout);
            }
        }
    }
    schedule();
    unlock_task();
    kassert(hd_op_append->result == -2 || hd_op_append->result == -1 || hd_op_append->result == 0);
    if(hd_op_append->result == -2)
    {
        cur_process->last_errno = ETIMEDOUT;
        return -1;
    }
    return hd_op_append->result;
}

int hd_read_blocks(struct block_buffer_desc_t* start, size_t count, int timeout)
{
    struct hd_operation_append_desc_t hd_op_append;
    hd_op_append.start = start;
    hd_op_append.count = count;
    hd_op_append.result = -2;
    hd_op_append.op_type = HD_OPERATION_READ;
    cur_process->waitlist_node.data = &hd_op_append;
    return hd_blocks_op(&hd_op_append, timeout);
}

int hd_write_blocks(struct block_buffer_desc_t* start, size_t count, int timeout)
{
    struct hd_operation_append_desc_t hd_op_append;
    hd_op_append.start = start;
    hd_op_append.count = count;
    hd_op_append.result = -2;
    hd_op_append.op_type = HD_OPERATION_WRITE;
    cur_process->waitlist_node.data = &hd_op_append;
    return hd_blocks_op(&hd_op_append, timeout);
}

void init_hd_info(struct hd_info_t* hd_info, int baseport, int slavebit)
{
    _memset(hd_info, 0, sizeof(*hd_info));
    hd_info->baseport = baseport;
    hd_info->slavebit = slavebit;
    hd_info->elevator.state = HD_ELEVATOR_UP;
    circular_list_init(&hd_info->elevator.up_head);
    circular_list_init(&hd_info->elevator.down_head);
    circular_list_init(&hd_info->elevator.free_head);
    circular_list_init(&hd_info->elevator.read_wait_list_head);
    circular_list_init(&hd_info->elevator.write_wait_list_head);
    for(size_t i = 0; i < HD_OP_CAPACITY; ++i)
        circular_list_insert(
            &hd_info->elevator.free_head,
            &hd_info->elevator.hd_opertations[i].node
        );
}

void init_hd_driver_module()
{
    _memset(hd_subdrivers, 0, sizeof(hd_subdrivers));
    init_hd_info(&hd_infos[0], 0x01f0, 0);
    int ret = load_hd_params(&hd_infos[0]);
    if(ret < 0)
        panic("load_hd_params fail\n");
    printk("hd0 lba len: %u blk = %u Byte = %u MB\n", hd_infos[0].len, hd_infos[0].len * 512, hd_infos[0].len * 512 / 1024 / 1024);
    ret = reset_hd_partition_table(&hd_infos[0]);
    kassert(ret == 0);
    int pc = load_hd_partition_table(&hd_infos[0], hd_subdrivers);
    if(pc < 0)
        panic("load partition fail\n");
    printk("has %d partition\n", pc);
    for(size_t i = 0; i < sizeof(hd_subdrivers) / sizeof(hd_subdrivers[0]); ++i)
        if(hd_subdrivers[i].baseport != 0)
            printk("partition %d: startlba [%u] len [%u]\n", i, hd_subdrivers[i].startlba, hd_subdrivers[i].len);
}
