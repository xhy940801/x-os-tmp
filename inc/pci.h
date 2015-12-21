#pragma once

#include "stdint.h"
#include "stddef.h"

union pci_configuration_space_desc_t
{
    struct
    {
        uint32_t datas[16];
    };
    struct
    {
        uint16_t vendor_id, device_id;
        uint16_t command, status;
        uint8_t revision_id, prog_if, subclass, class_code;
        uint8_t cache_line_size, latency_timer, header_type, bist;
        uint32_t bar0;
        uint32_t bar1;
        uint32_t bar2;
        uint32_t bar3;
        uint32_t bar4;
        uint32_t bar5;
        uint32_t cardbus_cis_pointer;
        uint16_t subsystem_vendor_id, subsystem_id;
        uint32_t expansion_rom_base_address;
        uint8_t capabilities_pointer, reserved0, reserved1, reserved2;
        uint32_t reserved3;
        uint8_t interrupt_line, interrupt_pin, min_grant, max_latency;
    };
};

struct pci_info_t
{
    uint32_t address_base;;
    uint16_t vendor_id, device_id;
};

#define PCI_CONFIG_ADDRESS  0xcf8
#define PCI_CONFIG_DATA     0xcfc

//int enumerating_pci_bus(struct pci_info_t* infos, size_t count);

void load_pci_configuration_space(union pci_configuration_space_desc_t* space, uint32_t address_base);

inline uint32_t pci_configuration_get_address(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset)
{
    uint32_t lbus  = (uint32_t)bus;
    uint32_t lslot = (uint32_t)slot;
    uint32_t lfunc = (uint32_t)func;
    return (uint32_t)((lbus << 16) | (lslot << 11) |
            (lfunc << 8) | (offset) | ((uint32_t)0x80000000));
}