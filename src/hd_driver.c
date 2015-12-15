#include "hd_driver.h"

#include "stddef.h"
#include "stdint.h"
#include "asm.h"
#include "panic.h"
#include "printk.h"

struct hd_subdriver_desc_t
{
    int baseport;
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
        ::"d"(port),"D"(p)
        :"memory"
    );
}

static inline void write256word(int port, const uint16_t* p)
{
    for(size_t i = 0; i < 256; ++i)
        __asm__ (
            "outw ax, dx"
            ::"d"(port),"a"(p[i])
        );
}

static int load_hd_params(struct hd_info_t* info)
{
    _outb(info->baseport + PORT_DRIVERSELECT, 0xa0 | info->slavebit);
    _outb(info->baseport + PORT_LBALO, 0);
    _outb(info->baseport + PORT_LBAMID, 0);
    _outb(info->baseport + PORT_LBAHI, 0);
    _outb(info->baseport + PORT_CMDSTATUS, 0xec);
    while(1)
    {
        int status = _inb(info->baseport + PORT_CMDSTATUS);
        if(status == 0)
            return -1;
        if(status & 0x80)
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

void init_hd_driver_module()
{
    struct hd_info_t hd_info;
    hd_info.baseport = 0x01f0;
    hd_info.slavebit = 0;
    int ret = load_hd_params(&hd_info);
    if(ret < 0)
        panic("load_hd_params fail\n");
    printk("hd0 lba len: %u blk = %u Byte = %u MB", hd_info.len, hd_info.len * 512, hd_info.len * 512 / 1024 / 1024);
}
