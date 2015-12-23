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
#include "pci.h"
#include "mem.h"
#include "interrupt.h"

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
    struct block_buffer_desc_t** block_buffers;
    size_t count;
    struct process_info_t* proc;
    enum hd_operation_type op_type;
};

struct hd_operation_append_desc_t
{
    struct block_buffer_desc_t** block_buffers;
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

struct prdt_desc_t
{
    uint32_t physical_address;
    uint16_t byte_count;
    uint16_t eot;
};

struct bmr_info_t
{
    int bmr_base_port;
    struct prdt_desc_t* prdt;
    uint32_t prdt_physical_address;
};

struct hd_info_t
{
    int baseport;
    int slavebit;
    int flags;
    size_t len;
    struct hd_elevator_desc_t elevator;
    struct bmr_info_t bmr;
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
static struct hd_info_t* last_op_hd;
static struct hd_info_t* last_op_hd;
static struct block_driver_desc_t hd_block_driver;

#define PORT_DATA           0x0000
#define PORT_SELECTORCOUNT  0x0002
#define PORT_LBALO          0x0003
#define PORT_LBAMID         0x0004
#define PORT_LBAHI          0x0005
#define PORT_DRIVERSELECT   0x0006
#define PORT_CMDSTATUS      0x0007
#define PORT_DRIVER_CTL     0x0206

#define PORT_BMI_M_COMMAND  0x0000
#define PORT_BMI_M_STATUS   0x0002
#define PORT_BMI_M_PRDTADDR 0x0004

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

int hd_irq_start_read(struct hd_info_t* hd_info, struct block_buffer_desc_t* block_buffers[], size_t count)
{
    for(size_t i = 0; i < count; ++i)
    {
        hd_info->bmr.prdt[i].physical_address = get_physical_addr((uint32_t) block_buffers[i]->buffer);
        hd_info->bmr.prdt[i].byte_count = 4096;
        hd_info->bmr.prdt[i].eot = 0x0000;
    }
    hd_info->bmr.prdt[count - 1].eot = 0x8000;

    _outd(hd_info->bmr.bmr_base_port + PORT_BMI_M_PRDTADDR, hd_info->bmr.prdt_physical_address);
    _outb(hd_info->bmr.bmr_base_port + PORT_BMI_M_COMMAND, 0x80);
    _outb(hd_info->bmr.bmr_base_port + PORT_BMI_M_STATUS, 0x06);

    uint32_t lba = block_buffers[0]->block_no << 2;
    _outb(hd_info->baseport + PORT_DRIVERSELECT, 0xe0 | hd_info->slavebit | ((lba >> 24) & 0x0f));
    _outb(hd_info->baseport + PORT_SELECTORCOUNT, count << 2);
    _outb(hd_info->baseport + PORT_LBALO, lba & 0xff);
    _outb(hd_info->baseport + PORT_LBAMID, ((lba >> 8) & 0xff));
    _outb(hd_info->baseport + PORT_LBAHI, ((lba >> 16) & 0xff));
    _outb(hd_info->baseport + PORT_CMDSTATUS, 0xc8);

    _outb(hd_info->bmr.bmr_base_port + PORT_BMI_M_COMMAND, 0x01);
    return 0;
}

int hd_irq_start_write(struct hd_info_t* hd_info, struct block_buffer_desc_t* block_buffers[], size_t count)
{
    for(size_t i = 0; i < count; ++i)
    {
        hd_info->bmr.prdt[i].physical_address = get_physical_addr((uint32_t) block_buffers[i]->buffer);
        hd_info->bmr.prdt[i].byte_count = 4096;
        hd_info->bmr.prdt[i].eot = 0x0000;
    }
    hd_info->bmr.prdt[count - 1].eot = 0x8000;

    _outd(hd_info->bmr.bmr_base_port + PORT_BMI_M_PRDTADDR, hd_info->bmr.prdt_physical_address);
    _outb(hd_info->bmr.bmr_base_port + PORT_BMI_M_COMMAND, 0x00);
    _outb(hd_info->bmr.bmr_base_port + PORT_BMI_M_STATUS, 0x06);

    uint32_t lba = block_buffers[0]->block_no << 2;
    _outb(hd_info->baseport + PORT_DRIVERSELECT, 0xe0 | hd_info->slavebit | ((lba >> 24) & 0x0f));
    _outb(hd_info->baseport + PORT_SELECTORCOUNT, count << 2);
    _outb(hd_info->baseport + PORT_LBALO, lba & 0xff);
    _outb(hd_info->baseport + PORT_LBAMID, ((lba >> 8) & 0xff));
    _outb(hd_info->baseport + PORT_LBAHI, ((lba >> 16) & 0xff));
    _outb(hd_info->baseport + PORT_CMDSTATUS, 0xca);

    _outb(hd_info->bmr.bmr_base_port + PORT_BMI_M_COMMAND, 0x01);
    return 0;
}

int hd_blocks_op(struct hd_operation_append_desc_t* hd_op_append, int timeout)
{
    struct block_buffer_desc_t** block_buffers = hd_op_append->block_buffers;
    size_t count = hd_op_append->count;
    kassert(count < 32);
    kassert(hd_op_append->op_type == HD_OPERATION_WRITE || hd_op_append->op_type == HD_OPERATION_READ);
    
    struct hd_info_t* hd_info = &hd_infos[block_buffers[0]->sub_driver >> 2];
    last_op_hd = hd_info;
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
        free_node->block_buffers = block_buffers;
        free_node->count = count;
        free_node->proc = cur_process;
        if(hd_info->elevator.state == HD_ELEVATOR_UP)
        {
            if(circular_list_is_empty(&hd_info->elevator.up_head))
            {
                kassert(circular_list_is_empty(&hd_info->elevator.down_head));
                int res;
                if(hd_op_append->op_type == HD_OPERATION_WRITE)
                    res = hd_irq_start_write(hd_info, block_buffers, count);
                else
                    res = hd_irq_start_read(hd_info, block_buffers, count);
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
                if(block_buffers[0]->block_no >
                    parentof(
                        hd_info->elevator.up_head.next,
                        struct hd_operation_desc_t,
                        node
                    )->block_buffers[0]->block_no
                )
                {
                    struct list_node_t* c_node = hd_info->elevator.up_head.next;
                    while(
                        c_node->next != &hd_info->elevator.up_head &&
                        block_buffers[0]->block_no > parentof(
                            c_node->next,
                            struct hd_operation_desc_t,
                            node
                        )->block_buffers[0]->block_no
                    )
                        c_node = c_node->next;
                    circular_list_insert(c_node->pre, &free_node->node);
                }
                else
                {
                    struct list_node_t* c_node = hd_info->elevator.down_head.next;
                    while(
                        c_node != &hd_info->elevator.down_head &&
                        block_buffers[0]->block_no < parentof(
                            c_node,
                            struct hd_operation_desc_t,
                            node
                        )->block_buffers[0]->block_no
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
                    res = hd_irq_start_write(hd_info, block_buffers, count);
                else
                    res = hd_irq_start_read(hd_info, block_buffers, count);
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
                if(block_buffers[0]->block_no <
                    parentof(
                        hd_info->elevator.down_head.next,
                        struct hd_operation_desc_t,
                        node
                    )->block_buffers[0]->block_no
                )
                {
                    struct list_node_t* c_node = hd_info->elevator.down_head.next;
                    while(
                        c_node->next != &hd_info->elevator.down_head &&
                        block_buffers[0]->block_no < parentof(
                            c_node->next,
                            struct hd_operation_desc_t,
                            node
                        )->block_buffers[0]->block_no
                    )
                        c_node = c_node->next;
                    circular_list_insert(c_node->pre, &free_node->node);
                }
                else
                {
                    struct list_node_t* c_node = hd_info->elevator.up_head.next;
                    while(
                        c_node != &hd_info->elevator.up_head &&
                        block_buffers[0]->block_no > parentof(
                            c_node,
                            struct hd_operation_desc_t,
                            node
                        )->block_buffers[0]->block_no
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

int hd_read_blocks(struct block_buffer_desc_t* block_buffers[], size_t count, int timeout)
{
    struct hd_operation_append_desc_t hd_op_append;
    hd_op_append.block_buffers = block_buffers;
    hd_op_append.count = count;
    hd_op_append.result = -2;
    hd_op_append.op_type = HD_OPERATION_READ;
    cur_process->waitlist_node.data = &hd_op_append;
    return hd_blocks_op(&hd_op_append, timeout);
}

int hd_write_blocks(struct block_buffer_desc_t* block_buffers[], size_t count, int timeout)
{
    struct hd_operation_append_desc_t hd_op_append;
    hd_op_append.block_buffers = block_buffers;
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

int enumerating_ata_controller(union pci_configuration_space_desc_t* pci_info)
{
    for(size_t bus = 0; bus < 256; ++bus)
        for(size_t slot = 0; slot < 32; ++slot)
            for(size_t function = 0; function < 8; ++function)
            {
                uint32_t address = pci_configuration_get_address(bus, slot, function, 0);
                load_pci_configuration_space(pci_info, address);
                if(pci_info->vendor_id != 0xffff && pci_info->class_code == 0x01 && pci_info->subclass == 0x01)
                    return 0;
            }
    return -1;
}

void init_hd_pci_info(struct hd_info_t* hd_info)
{
    union pci_configuration_space_desc_t pci_info;
    int ret = enumerating_ata_controller(&pci_info);
    kassert(ret == 0);
    kassert(pci_info.bar4 & 0x01);
    hd_info->bmr.bmr_base_port = pci_info.bar4 & (0xfffffffc);
    hd_info->bmr.prdt = kgetpersistedpage(1);
    hd_info->bmr.prdt_physical_address = get_physical_addr((uint32_t) hd_info->bmr.prdt);

    hd_block_driver.read_blocks = hd_read_blocks;
    hd_block_driver.write_blocks = hd_write_blocks;
    hd_block_driver.max_read = 32;
    hd_block_driver.max_write = 32;
    block_driver_register(&hd_block_driver, 3);
}

static inline void set_hd_op_result(struct hd_operation_desc_t* op, int result)
{
    struct hd_operation_append_desc_t* op_append = (struct hd_operation_append_desc_t*) op->proc->waitlist_node.data;
    op_append->result = result;
}

static inline uint32_t get_ata_lba(int baseport)
{
    uint32_t lba = ((uint32_t) _inb(baseport + PORT_DRIVERSELECT)) & 0x0f;
    lba <<= 8;
    lba |= ((uint32_t) _inb(baseport + PORT_LBAHI)) & 0xff;
    lba <<= 8;
    lba |= ((uint32_t) _inb(baseport + PORT_LBAMID)) & 0xff;
    lba <<= 8;
    lba |= ((uint32_t) _inb(baseport + PORT_LBALO)) & 0xff;
    return lba;
}

void do_hd_irq()
{
    v_lock_task();
    struct hd_info_t* hd_info = last_op_hd;
    int dma_status = _inb(hd_info->bmr.bmr_base_port + PORT_BMI_M_STATUS);
    int ata_status = _inb(hd_info->baseport + PORT_CMDSTATUS);
    struct hd_operation_desc_t* cur_op;
    struct hd_operation_desc_t* next_op;
    _outb(hd_info->bmr.bmr_base_port + PORT_BMI_M_COMMAND, 0x00);
    kassert(hd_info->elevator.state == HD_ELEVATOR_UP || hd_info->elevator.state == HD_ELEVATOR_DOWN);
    if(hd_info->elevator.state == HD_ELEVATOR_UP)
    {
        kassert(!circular_list_is_empty(&hd_info->elevator.up_head));
        cur_op = parentof(hd_info->elevator.up_head.next, struct hd_operation_desc_t, node);
        circular_list_remove(&hd_info->elevator.up_head);
        if(circular_list_is_empty(&hd_info->elevator.up_head))
        {
            hd_info->elevator.state = HD_ELEVATOR_DOWN;
            if(circular_list_is_empty(&hd_info->elevator.down_head))
                next_op = NULL;
            else
                next_op = parentof(hd_info->elevator.down_head.next, struct hd_operation_desc_t, node);
        }
        else
            next_op = parentof(hd_info->elevator.up_head.next, struct hd_operation_desc_t, node);
    }
    else
    {
        kassert(!circular_list_is_empty(&hd_info->elevator.down_head));
        cur_op = parentof(hd_info->elevator.down_head.next, struct hd_operation_desc_t, node);
        circular_list_remove(&hd_info->elevator.down_head);
        if(circular_list_is_empty(&hd_info->elevator.down_head))
        {
            hd_info->elevator.state = HD_ELEVATOR_UP;
            if(circular_list_is_empty(&hd_info->elevator.up_head))
                next_op = NULL;
            else
                next_op = parentof(hd_info->elevator.up_head.next, struct hd_operation_desc_t, node);
        }
        else
            next_op = parentof(hd_info->elevator.down_head.next, struct hd_operation_desc_t, node);
    }

    kwakeup(cur_op->proc);
    in_sched_queue(cur_op->proc);
    if(dma_status & 0x02 || ata_status & 0x01)
    {
        uint32_t lba = get_ata_lba(hd_info->baseport);
        size_t block_no = lba >> 2;
        kassert(block_no >= cur_op->block_buffers[0]->block_no);
        if(block_no == cur_op->block_buffers[0]->block_no)
            set_hd_op_result(cur_op, -1);
        else
            set_hd_op_result(cur_op, block_no - cur_op->block_buffers[0]->block_no);
    }
    else
    {
        uint32_t lba = get_ata_lba(hd_info->baseport);
        kassert(lba == (cur_op->block_buffers[0]->block_no << 2) + (cur_op->count << 2));
        set_hd_op_result(cur_op, cur_op->count);
    }

    if(next_op != NULL)
    {
        int res;
        kassert(next_op->op_type == HD_OPERATION_READ || next_op->op_type == HD_OPERATION_WRITE);
        kwakeup(next_op->proc);
        if(next_op->op_type == HD_OPERATION_READ)
            res = hd_irq_start_read(hd_info, next_op->block_buffers, next_op->count);
        else
            res = hd_irq_start_write(hd_info, next_op->block_buffers, next_op->count);
        if(res < 0)
        {
            in_sched_queue(next_op->proc);
            set_hd_op_result(next_op, -1);
        }
        else
            kuninterruptwait(next_op->proc);
    }
    v_unlock_task();
}

/*struct prdt_desc_t* prdt;
void do_hd_irq()
{
    static int irqc = 0;
    v_lock_task();
    printk("CCCCCCCCCCCCCCCCC\n");
    if(irqc == 0)
    {
        struct hd_info_t* info = &hd_infos[0];
        int status = _inb(bmr_base_port + PORT_BMI_M_STATUS);
        int dstatus = _inb(info->baseport + PORT_CMDSTATUS);
        uint32_t lbalo = _inb(info->baseport + PORT_LBALO);
        uint32_t lbamid = _inb(info->baseport + PORT_LBAMID);
        uint32_t lbahi = _inb(info->baseport + PORT_LBAHI);
        printk("st0 [%u] st1 [%u] lba [%u] [%u] [%u]\n", status, dstatus, lbalo, lbamid, lbahi);
        uint32_t* data = (uint32_t*) kgetpersistedpage(1);
        for(size_t i = 0; i < 8; ++i)
        {
            int ret = hd_pio_readoneblock(info, 0x600 + i, (char*) (data + 128 * i));
            if(ret != 0)
                printk("at read i [%u]\n", i);
        }
        for(size_t i = 0; i < 4096 / sizeof(data[0]); ++i)
        {
            if(data[i] != i)
            {
                printk("at data[%u] = %u\n", i, data[i]);
                panic("");
            }
        }
        [/ *]data = (uint32_t*) kgetpersistedpage(1);
        for(size_t i = 0; i < 4096 / sizeof(data[0]); ++i)
            data[i] = i + 1024;
        prdt[1].physical_address = get_physical_addr((uint32_t) data);
        prdt[1].byte_count = 4096;
        prdt[1].eot = 0x8000;
        uint32_t lba = 0x500;
        _outb(info->baseport + PORT_DRIVERSELECT, 0xe0 | info->slavebit | ((lba >> 24) & 0x0f));
        _outb(info->baseport + PORT_SELECTORCOUNT, 8);
        _outb(info->baseport + PORT_LBALO, lba & 0xff);
        _outb(info->baseport + PORT_LBAMID, ((lba >> 8) & 0xff));
        _outb(info->baseport + PORT_LBAHI, ((lba >> 16) & 0xff));
        _outb(info->baseport + PORT_CMDSTATUS, 0xca);
        ++irqc;*//*
        _outb(0xa0, 0x20);
        _outb(0x20, 0x20);
        panic("");
    }
    else
    {
        struct hd_info_t* info = &hd_infos[0];
        int status = _inb(bmr_base_port + PORT_BMI_M_STATUS);
        int dstatus = _inb(info->baseport + PORT_CMDSTATUS);
        uint32_t lbalo = _inb(info->baseport + PORT_LBALO);
        uint32_t lbamid = _inb(info->baseport + PORT_LBAMID);
        uint32_t lbahi = _inb(info->baseport + PORT_LBAHI);
        printk("st0 [%u] st1 [%u] lba [%u] [%u] [%u]\n", status, dstatus, lbalo, lbamid, lbahi);
        uint32_t* data = (uint32_t*) kgetpersistedpage(1);
        for(size_t i = 0; i < 8; ++i)
        {
            int ret = hd_pio_readoneblock(info, 0x500 + i, (char*) (data + 128 * i));
            if(ret != 0)
                printk("at read i [%u]\n", i);
        }
        for(size_t i = 0; i < 4096 / sizeof(data[0]); ++i)
        {
            if(data[i] != i + 1024)
            {
                printk("at data[%u] = %u\n", i, data[i]);
                panic("");
            }
        }
        panic("");
    }

    v_unlock_task();
}*/
/*
void test_dma_write()
{
    prdt = (struct prdt_desc_t*) kgetpersistedpage(1);
    uint32_t* data = (uint32_t*) kgetpersistedpage(1);
    for(size_t i = 0; i < 4096 / sizeof(data[0]); ++i)
        data[i] = i;
    prdt[0].physical_address = get_physical_addr((uint32_t) data);
    prdt[0].byte_count = 2048;
    prdt[0].eot = 0x0000;
    prdt[1].physical_address = prdt[0].physical_address + 2048;
    prdt[1].byte_count = 2048;
    prdt[1].eot = 0x8000;

    uint32_t prdt_physical_addr = get_physical_addr((uint32_t) prdt);
    _outd(bmr_base_port + PORT_BMI_M_PRDTADDR, prdt_physical_addr);
    _outb(bmr_base_port + PORT_BMI_M_COMMAND, 0x00);
    _outb(bmr_base_port + PORT_BMI_M_STATUS, 0x06);

    struct hd_info_t* info = &hd_infos[0];

    uint32_t lba = 0x600;
    _outb(info->baseport + PORT_DRIVERSELECT, 0xe0 | info->slavebit | ((lba >> 24) & 0x0f));
    _outb(info->baseport + PORT_SELECTORCOUNT, 4);
    _outb(info->baseport + PORT_LBALO, lba & 0xff);
    _outb(info->baseport + PORT_LBAMID, ((lba >> 8) & 0xff));
    _outb(info->baseport + PORT_LBAHI, ((lba >> 16) & 0xff));
    _outb(info->baseport + PORT_CMDSTATUS, 0xca);

    _outb(bmr_base_port + PORT_BMI_M_COMMAND, 0x01);
}*/

void on_hd_interrupt_request();

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
    init_hd_pci_info(&hd_infos[0]);
    clear_8259_mask(14);
    setup_intr_desc(0x2e, on_hd_interrupt_request, 0);
    //test_dma_write();
}
