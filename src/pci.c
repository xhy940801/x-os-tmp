#include "pci.h"

#include "panic.h"
#include "string.h"
#include "asm.h"

uint32_t pci_configuration_read_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset)
{
    uint32_t address = pci_configuration_get_address(bus, slot, func, offset);
    _outd(0xcf8, address);
    return _ind(0xcfc);
}

/*int enumerating_pci_bus(struct pci_info_t* infos, size_t count)
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
*/

void load_pci_configuration_space(union pci_configuration_space_desc_t* space, uint32_t address_base)
{
    kassert((address_base & 0xff) == 0);
    kassert(address_base & 0x80000000);
    for(size_t i = 0; i < sizeof(*space) / sizeof(uint32_t); ++i)
    {
        _outd(PCI_CONFIG_ADDRESS, address_base | (i << 2));
        space->datas[i] = _ind(PCI_CONFIG_DATA);
    }
}
