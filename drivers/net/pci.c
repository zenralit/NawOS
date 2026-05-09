#include "pci.h"
#include "drivers/ports/ports.h"

static uint32_t pci_config_address(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    return (1 << 31) | ((uint32_t)bus << 16) | ((uint32_t)slot << 11) |
           ((uint32_t)func << 8) | (offset & 0xFC);
}

uint32_t pci_config_read_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    outl(0xCF8, pci_config_address(bus, slot, func, offset));
    return inl(0xCFC);
}

void pci_config_write_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value) {
    outl(0xCF8, pci_config_address(bus, slot, func, offset));
    outl(0xCFC, value);
}

uint16_t pci_config_read_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t value = pci_config_read_dword(bus, slot, func, offset & 0xFC);
    return (value >> ((offset & 2) * 8)) & 0xFFFF;
}

uint8_t pci_config_read_byte(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t value = pci_config_read_dword(bus, slot, func, offset & 0xFC);
    return (value >> ((offset & 3) * 8)) & 0xFF;
}

void pci_config_write_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t value) {
    uint8_t aligned_offset = offset & 0xFC;
    uint32_t current = pci_config_read_dword(bus, slot, func, aligned_offset);
    uint32_t shift = (offset & 2) * 8;
    current &= ~(0xFFFFu << shift);
    current |= ((uint32_t)value) << shift;
    pci_config_write_dword(bus, slot, func, aligned_offset, current);
}

pci_device_t pci_find_device(uint16_t vendor_id, uint16_t device_id) {
    for (uint8_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            for (uint8_t func = 0; func < 8; func++) {
                uint16_t vid = pci_config_read_word(bus, slot, func, 0);
                if (vid == 0xFFFF) continue;
                uint16_t did = pci_config_read_word(bus, slot, func, 2);
                if (vid == vendor_id && did == device_id) {
                    return (pci_device_t){ bus, slot, func, vid, did };
                }
            }
        }
    }
    return (pci_device_t){ 0xFF, 0xFF, 0xFF, 0xFFFF, 0xFFFF };
}

uint32_t pci_get_bar(uint8_t bus, uint8_t slot, uint8_t func, uint8_t bar_num) {
    return pci_config_read_dword(bus, slot, func, 0x10 + bar_num * 4);
}

uint8_t pci_get_irq_line(uint8_t bus, uint8_t slot, uint8_t func) {
    return pci_config_read_byte(bus, slot, func, 0x3C);
}

void pci_enable_bus_mastering(uint8_t bus, uint8_t slot, uint8_t func) {
    uint16_t command = pci_config_read_word(bus, slot, func, 0x04);
    command |= 0x0005;
    pci_config_write_word(bus, slot, func, 0x04, command);
}
