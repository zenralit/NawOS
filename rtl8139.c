#include "rtl8139.h"
#include "ports.h"
#include "screen.h"
#include "pci.h"

#define RTL_VENDOR_ID 0x10EC
#define RTL_DEVICE_ID 0x8139

#define RTL_COMMAND_REG 0x37
#define RTL_MAC_REG     0x00

uint8_t rtl_mac[6];

void rtl8139_init() {
    pci_device_t dev = pci_find_device(RTL_VENDOR_ID, RTL_DEVICE_ID);
    if (dev.vendor_id == 0xFFFF) {
        print("RTL8139 not found.\n");
        return;
    }

    // print("RTL8139 detected\n");

    uint32_t io_base = pci_get_bar(dev.bus, dev.slot, dev.func, 0) & ~0x3;
    print("I/O base: ");
    print_hex(io_base);
    print("\n");


    outb(io_base + RTL_COMMAND_REG, 0x10);
    for (int i = 0; i < 10000; i++);


    for (int i = 0; i < 6; i++) {
        rtl_mac[i] = inb(io_base + RTL_MAC_REG + i);
    }

    print("MAC: ");
    for (int i = 0; i < 6; i++) {
        print_hex(rtl_mac[i]);
        if (i < 5) print(":");
    }
    print("\n");
}
