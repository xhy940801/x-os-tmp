#include "pci.h"

#include "panic.h"
#include "string.h"
#include "asm.h"

static inline uint32_t pci_configuration_get_address(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset)
{
    kassert((offset & 0x03) == 0);
    uint32_t lbus  = (uint32_t)bus;
    uint32_t lslot = (uint32_t)slot;
    uint32_t lfunc = (uint32_t)func;
    return (uint32_t)((lbus << 16) | (lslot << 11) |
            (lfunc << 8) | (offset) | ((uint32_t)0x80000000));
}

uint32_t pci_configuration_read_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset)
{
    uint32_t address = pci_configuration_get_address(bus, slot, func, offset);
    _outd(0xcf8, address);
    return _ind(0xcfc);
}

int enumerating_pci_bus(struct pci_info_t* infos, size_t count)
{
    _memset(infos, 0, sizeof(*infos) * count);
    int rs = 0;
    for(uint8_t bus = 0; bus <= 255; ++bus)
    {
        for(uint8_t slot = 0; slot < 32; ++slot)
        {
            uint32_t off0 = pci_configuration_read_dword(bus, slot, 0, 0);
            uint16_t vendor_id = off0 & 0xffff;
            uint16_t device_id = off0 >> 16;
            if(vendor_id == 0xffff)
                continue;
            uint32_t hehe = pci_configuration_read_dword(bus, slot, 0, 0x08);
            printk("has ??????????????????? [%u] [%u] [%u] [%u] [%u]\n", bus, slot, vendor_id, device_id, hehe >> 16);
            for(size_t i = 0; i < count; ++i)
            {
                if(infos[i].address_base & 0x80000000)
                    continue;
                if(vendor_id == infos[i].vendor_id && device_id == infos[i].device_id)
                {
                    infos[i].address_base = pci_configuration_get_address(bus, slot, 0, 0);
                    ++rs;
                    break;
                }
            }
        }
        if(bus == 255)
            break;
    }
    return rs;
}
