#include "rtl8139.h"
#include "drivers/ports/ports.h"
#include "drivers/screen/screen.h"
#include "pci.h"
#include "dhcp.h"
#include <string.h>

static uint32_t io_base = 0;

#define RTL_VENDOR_ID 0x10EC
#define RTL_DEVICE_ID 0x8139

#define RX_BUFFER_SIZE 8192
#define RTL_RX_BUFFER_PTR 0x38
#define RTL_ISR           0x3E
#define RTL_CMD_RECEIVE_OK 0x01

#define RTL_COMMAND_REG   0x37
#define RTL_MAC_REG       0x00
#define RTL_RX_BUF        0x30
#define RTL_RX_CONFIG     0x44
#define RTL_TX_CONFIG     0x40
#define RTL_IMR           0x3C
#define RTL_ISR           0x3E
#define RTL_TX_ADDR0      0x20
#define RTL_TX_STAT0      0x10
#define RTL_CMD_RX_ENABLE 0x08
#define RTL_CMD_TX_ENABLE 0x04
#define RTL_CMD_REG       0x37


volatile int net_packet_received = 0;

void set_net_packet_flag() {
    net_packet_received = 1;
}
static uint8_t rx_buffer[RX_BUFFER_SIZE + 16 + 1500];
static uint32_t rx_cur = 0;


uint8_t rtl_mac[6];
//uint8_t rx_buffer[8192 + 16 + 1500] __attribute__((aligned(256)));


uint8_t* rtl8139_recv_packet(uint32_t* size) {
    uint16_t status = inw(io_base + RTL_ISR);
    if (!(status & RTL_CMD_RECEIVE_OK)) {
        return 0;
    }


    uint32_t offset = rx_cur;
    uint32_t packet_header = *(uint32_t*)(rx_buffer + offset);
    uint16_t pkt_len = packet_header & 0xFFFF;

    uint8_t* packet = rx_buffer + offset + 4; 
    *size = pkt_len;


    rx_cur = (rx_cur + pkt_len + 4 + 3) & ~3;
    outw(io_base + RTL_RX_BUFFER_PTR, rx_cur - 16);


    outw(io_base + RTL_ISR, RTL_CMD_RECEIVE_OK);

    return packet;
}

void rtl8139_send_dhcp_discover() {
    uint8_t packet[548] = {0};

    
    memset(packet, 0xFF, 6); 
    memcpy(&packet[6], rtl_mac, 6); 
    packet[12] = 0x08;
    packet[13] = 0x00;

    
    packet[14] = 0x45;
    packet[16] = 0x02; packet[17] = 0x28; 
    packet[23] = 0x11; 
    packet[26] = 0x00; packet[27] = 0x00; packet[28] = 0x00; packet[29] = 0x00; 
    packet[30] = 0xFF; packet[31] = 0xFF; packet[32] = 0xFF; packet[33] = 0xFF; 

  
    packet[34] = 0x00; packet[35] = 0x44;
    packet[36] = 0x00; packet[37] = 0x43;
    packet[38] = 0x02; packet[39] = 0x14;

    
    packet[42] = 0x01; 
    packet[43] = 0x01; 
    packet[44] = 0x06; 
    packet[46] = 0x39; packet[47] = 0x03; 
    memcpy(&packet[50], rtl_mac, 6); 
    packet[236] = 0x63; packet[237] = 0x82; packet[238] = 0x53; packet[239] = 0x63; 

    packet[240] = 53; packet[241] = 1; packet[242] = 1; 
    packet[243] = 255; 

    rtl8139_send_packet(packet, 548);
}

void rtl8139_init() {
        outw(io_base + 0x3C, 0x0005);
    pci_device_t dev = pci_find_device(RTL_VENDOR_ID, RTL_DEVICE_ID);
    if (dev.vendor_id == 0xFFFF) {
        print("RTL8139 not found.\n");
        return;
    }

    io_base = pci_get_bar(dev.bus, dev.slot, dev.func, 0) & ~0x3;

    // print("I/O base: ");
    // print_hex(io_base);
    // print("\n");

    outb(io_base + RTL_COMMAND_REG, 0x10);
    for (int i = 0; i < 10000; i++);

    for (int i = 0; i < 6; i++) {
        rtl_mac[i] = inb(io_base + RTL_MAC_REG + i);
    }

    // print("MAC: ");
    // for (int i = 0; i < 6; i++) {
    //     print_hex(rtl_mac[i]);
    //     if (i < 5) print(":");
    // }
    // print("\n");

    outl(io_base + RTL_RX_BUF, (uint32_t)&rx_buffer[0]);
    outb(io_base + RTL_CMD_REG, RTL_CMD_RX_ENABLE | RTL_CMD_TX_ENABLE);
    outl(io_base + RTL_RX_CONFIG, 0xf | (1 << 7) | (1 << 3));
    outw(io_base + RTL_IMR, 0x0005);

    // print("RTL8139 initialized and ready.\n");
}

void rtl8139_poll() {
    uint16_t status = inw(io_base + RTL_ISR);
    if (status & 0x01) {
        print("Packet received\n");
        outw(io_base + RTL_ISR, 0x01);
    }
}

void rtl8139_send_packet(void* data, uint32_t length) {
    if (length > 1792) return;

    outl(io_base + RTL_TX_ADDR0, (uint32_t)data);
    outl(io_base + RTL_TX_STAT0, length);
}
void rtl8139_handle_receive() {
    while (1) {
        uint32_t size = 0;
        uint8_t* packet = rtl8139_recv_packet(&size);
        if (!packet) return;

        if (packet[12] == 0x08 && packet[13] == 0x00) { // IP
            uint8_t protocol = packet[23];
            if (protocol == 17) { // UDP
                uint16_t src_port = (packet[34] << 8) | packet[35];
                uint16_t dst_port = (packet[36] << 8) | packet[37];
                if (src_port == 67 && dst_port == 68) {
                    parse_dhcp_offer(packet, size);
                }
            }
        }
    }
}
