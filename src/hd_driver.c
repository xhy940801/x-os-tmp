#include "hd_driver.h"

#include "stddef.h"
#include "stdint.h"
#include "asm.h"
#include "panic.h"
#include "printk.h"
#include "string.h"

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

void init_hd_driver_module()
{
    _memset(hd_subdrivers, 0, sizeof(hd_subdrivers));
    struct hd_info_t hd_info;
    hd_info.baseport = 0x01f0;
    hd_info.slavebit = 0;
    int ret = load_hd_params(&hd_info);
    if(ret < 0)
        panic("load_hd_params fail\n");
    printk("hd0 lba len: %u blk = %u Byte = %u MB\n", hd_info.len, hd_info.len * 512, hd_info.len * 512 / 1024 / 1024);
    ret = reset_hd_partition_table(&hd_info);
    kassert(ret == 0);
    int pc = load_hd_partition_table(&hd_info, hd_subdrivers);
    if(pc < 0)
        panic("load partition fail\n");
    printk("has %d partition\n", pc);
    for(size_t i = 0; i < sizeof(hd_subdrivers) / sizeof(hd_subdrivers[0]); ++i)
        if(hd_subdrivers[i].baseport != 0)
            printk("partition %d: startlba [%u] len [%u]\n", i, hd_subdrivers[i].startlba, hd_subdrivers[i].len);
}
