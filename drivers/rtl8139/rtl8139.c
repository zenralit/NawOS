#include "drivers/rtl8139/rtl8139.h"
#include "drivers/pci/pci.h"
#include "drivers/ports/ports.h"
#include "kernel/memory/memory.h"
#include "kernel/terminal/terminal.h"
#include "drivers/net/network.h"
#include "net/ethernet.h"
#include "net/net_limits.h"

#define RTL_VENDOR_ID             0x10EC
#define RTL_DEVICE_ID             0x8139

#define RTL_RX_RING_SIZE          8192
#define RTL_RX_BUFFER_SIZE        (RTL_RX_RING_SIZE + 16)
#define RTL_TX_BUFFER_COUNT       4
#define RTL_TX_BUFFER_SIZE        NET_ETHERNET_FRAME_MAX_SIZE
#define RTL_RX_MAX_PACKET_SIZE    NET_ETHERNET_FRAME_WITH_FCS_MAX_SIZE

#define RTL_REG_MAC0              0x00
#define RTL_REG_TX_STATUS0        0x10
#define RTL_REG_TX_ADDR0          0x20
#define RTL_REG_RX_BUF            0x30
#define RTL_REG_CAPR              0x38
#define RTL_REG_IMR               0x3C
#define RTL_REG_ISR               0x3E
#define RTL_REG_TCR               0x40
#define RTL_REG_RCR               0x44
#define RTL_REG_CONFIG1           0x52
#define RTL_REG_COMMAND           0x37

#define RTL_CMD_RX_ENABLE         0x08
#define RTL_CMD_TX_ENABLE         0x04
#define RTL_CMD_RESET             0x10
#define RTL_CMD_RX_BUFFER_EMPTY   0x01

#define RTL_ISR_RX_OK             0x0001
#define RTL_ISR_RX_ERR            0x0002
#define RTL_ISR_TX_OK             0x0004
#define RTL_ISR_TX_ERR            0x0008
#define RTL_ISR_RX_OVERFLOW       0x0010

#define RTL_RCR_ACCEPT_ALL_PHYS   0x00000001
#define RTL_RCR_ACCEPT_MY_PHYS    0x00000002
#define RTL_RCR_ACCEPT_MULTICAST  0x00000004
#define RTL_RCR_ACCEPT_BROADCAST  0x00000008
#define RTL_RCR_WRAP              0x00000080

#define RTL_RCR_RBLEN_8K          0x00000000

#define RTL_RX_STATUS_OK          0x0001
#define RTL_TX_STATUS_OWN          0x00002000

static uint32_t io_base = 0;
static int rtl_ready = 0;
static uint32_t rx_cur = 0;
static uint8_t tx_slot = 0;
static uint8_t tx_busy[RTL_TX_BUFFER_COUNT];

uint8_t rtl_mac[6];

static uint8_t rx_buffer[RTL_RX_BUFFER_SIZE] __attribute__((aligned(4096)));
static uint8_t tx_buffers[RTL_TX_BUFFER_COUNT][RTL_TX_BUFFER_SIZE] __attribute__((aligned(16)));
static uint8_t rx_frame[NET_ETHERNET_FRAME_MAX_SIZE];

int rtl8139_is_ready() {
    return rtl_ready;
}

static void rtl8139_acknowledge(uint16_t status) {
    if (!rtl_ready) {
        return;
    }
    outw(io_base + RTL_REG_ISR, status);
}

void rtl8139_poll() {
    if (!rtl_ready) {
        return;
    }

    uint16_t status = inw(io_base + RTL_REG_ISR);
    uint8_t command = inb(io_base + RTL_REG_COMMAND);

    if (status & (RTL_ISR_TX_OK | RTL_ISR_TX_ERR)) {
        rtl8139_acknowledge(status & (RTL_ISR_TX_OK | RTL_ISR_TX_ERR));
    }

    for (uint8_t i = 0; i < RTL_TX_BUFFER_COUNT; i++) {
        uint32_t tx_status = inl(io_base + RTL_REG_TX_STATUS0 + i * 4);
        if (tx_busy[i] && (tx_status & RTL_TX_STATUS_OWN)) {
            tx_busy[i] = 0;
        }
    }

    if ((status & (RTL_ISR_RX_OK | RTL_ISR_RX_ERR | RTL_ISR_RX_OVERFLOW)) ||
        !(command & RTL_CMD_RX_BUFFER_EMPTY)) {
        network_set_packet_flag();
    }
}

void rtl8139_handle_irq() {
    if (!rtl_ready) {
        return;
    }

    uint16_t status = inw(io_base + RTL_REG_ISR);
    if (status & (RTL_ISR_RX_OK | RTL_ISR_RX_ERR | RTL_ISR_RX_OVERFLOW)) {
        network_set_packet_flag();
    }

    if (status & (RTL_ISR_RX_OK | RTL_ISR_RX_ERR | RTL_ISR_RX_OVERFLOW | RTL_ISR_TX_OK | RTL_ISR_TX_ERR)) {
        outw(io_base + RTL_REG_ISR, status);
    }
}

int rtl8139_init() {
    pci_device_t dev = pci_find_device(RTL_VENDOR_ID, RTL_DEVICE_ID);

    rtl_ready = 0;
    io_base = 0;
    if (dev.vendor_id == 0xFFFF) {
        return 0;
    }

    pci_enable_bus_mastering(dev.bus, dev.slot, dev.func);
    io_base = pci_get_bar(dev.bus, dev.slot, dev.func, 0) & ~0x3;
    if (io_base == 0 || io_base == 0xFFFFFFFC ||
        (pci_get_bar(dev.bus, dev.slot, dev.func, 0) & 1) == 0) {
        terminal_write("RTL8139 has an invalid I/O BAR.\n");
        io_base = 0;
        return 0;
    }

    outb(io_base + RTL_REG_CONFIG1, 0x00);
    outb(io_base + RTL_REG_COMMAND, RTL_CMD_RESET);
    uint32_t reset_timeout = 10000000;
    while ((inb(io_base + RTL_REG_COMMAND) & RTL_CMD_RESET) && reset_timeout > 0) {
        reset_timeout--;
    }
    if (reset_timeout == 0) {
        terminal_write("RTL8139 reset timed out.\n");
        io_base = 0;
        return 0;
    }

    for (int i = 0; i < 6; i++) {
        rtl_mac[i] = inb(io_base + RTL_REG_MAC0 + i);
    }

    rx_cur = 0;
    tx_slot = 0;
    memset(rx_buffer, 0, sizeof(rx_buffer));
    memset(tx_buffers, 0, sizeof(tx_buffers));
    memset(tx_busy, 0, sizeof(tx_busy));

    outl(io_base + RTL_REG_RX_BUF, (uint32_t)rx_buffer);
    outl(io_base + RTL_REG_TCR, 0x03000600);
    outl(io_base + RTL_REG_RCR,
         RTL_RCR_ACCEPT_ALL_PHYS |
         RTL_RCR_ACCEPT_MY_PHYS |
         RTL_RCR_ACCEPT_MULTICAST |
         RTL_RCR_ACCEPT_BROADCAST |
         RTL_RCR_RBLEN_8K |
         RTL_RCR_WRAP);

    outw(io_base + RTL_REG_CAPR, 0xFFF0);
    outw(io_base + RTL_REG_IMR, RTL_ISR_RX_OK | RTL_ISR_RX_ERR | RTL_ISR_TX_OK | RTL_ISR_TX_ERR);

    outb(io_base + RTL_REG_COMMAND, RTL_CMD_RX_ENABLE | RTL_CMD_TX_ENABLE);

    rtl_ready = 1;
    rtl8139_acknowledge(0xFFFF);

    terminal_write("RTL8139 MAC: ");
    for (int i = 0; i < 6; i++) {
        terminal_write_hex_byte(rtl_mac[i]);
        if (i < 5) {
            terminal_write(":");
        }
    }
    terminal_write(" IRQ ");
    terminal_write_dec(pci_get_irq_line(dev.bus, dev.slot, dev.func));
    terminal_write("\n");
    return 1;
}

int rtl8139_send_packet(const void* data, uint32_t length) {
    uint8_t slot;

    if (!rtl_ready || data == 0 || length == 0 || length > RTL_TX_BUFFER_SIZE) {
        return 0;
    }

    for (uint8_t i = 0; i < RTL_TX_BUFFER_COUNT; i++) {
        slot = (tx_slot + i) % RTL_TX_BUFFER_COUNT;
        if (!tx_busy[slot]) {
            tx_slot = (slot + 1) % RTL_TX_BUFFER_COUNT;
            memcpy(tx_buffers[slot], data, length);
            outl(io_base + RTL_REG_TX_ADDR0 + slot * 4, (uint32_t)tx_buffers[slot]);
            tx_busy[slot] = 1;
            __asm__ volatile("" ::: "memory");
            outl(io_base + RTL_REG_TX_STATUS0 + slot * 4, length & 0x1FFF);
            return 1;
        }
    }

    return 0;
}

#define RTL_REG_CBR 0x3A

static void rtl8139_copy_from_rx_ring(uint8_t* dest, uint32_t offset, uint32_t length) {
    uint32_t first = RTL_RX_RING_SIZE - offset;
    if (first > length) {
        first = length;
    }

    memcpy(dest, rx_buffer + offset, first);
    if (length > first) {
        memcpy(dest + first, rx_buffer, length - first);
    }
}

static void rtl8139_advance_rx_cursor(uint32_t packet_len) {
    rx_cur = (rx_cur + packet_len + 7) & ~3U;
    rx_cur %= RTL_RX_RING_SIZE;

    uint32_t capr_value = (rx_cur - 0x10 + RTL_RX_RING_SIZE) % RTL_RX_RING_SIZE;
    outw(io_base + RTL_REG_CAPR, (uint16_t)capr_value);
}

void rtl8139_handle_receive() {
    if (!rtl_ready) {
        return;
    }

    network_packet_received = 0;
    int max_packets = 32;

    while (!(inb(io_base + RTL_REG_COMMAND) & RTL_CMD_RX_BUFFER_EMPTY)) {
        if (max_packets-- <= 0) {
            break;
        }

        uint32_t offset = rx_cur % RTL_RX_RING_SIZE;

        __asm__ volatile("" ::: "memory");

        volatile uint16_t* status_ptr = (volatile uint16_t*)(rx_buffer + offset);
        volatile uint16_t* len_ptr = (volatile uint16_t*)(rx_buffer + offset + 2);

        int timeout = 100;
        while (*status_ptr == 0 && *len_ptr == 0 && timeout > 0) {
            __asm__ volatile("pause");
            timeout--;
        }

        uint16_t status = *status_ptr;
        uint16_t packet_len = *len_ptr;

        if (status == 0 && packet_len == 0) {
            uint16_t cbr = inw(io_base + RTL_REG_CBR);
            rx_cur = ((uint32_t)cbr + 3) & ~3U;
            rx_cur %= RTL_RX_RING_SIZE;
            uint32_t capr_value = (rx_cur - 0x10 + RTL_RX_RING_SIZE) % RTL_RX_RING_SIZE;
            outw(io_base + RTL_REG_CAPR, (uint16_t)capr_value);
            break;
        }

        if (!(status & RTL_RX_STATUS_OK) || packet_len < 4 ||
            packet_len > RTL_RX_MAX_PACKET_SIZE) {
            uint16_t cbr = inw(io_base + RTL_REG_CBR);
            rx_cur = ((uint32_t)cbr + 3) & ~3U;
            rx_cur %= RTL_RX_RING_SIZE;
            uint32_t capr_value = (rx_cur - 0x10 + RTL_RX_RING_SIZE) % RTL_RX_RING_SIZE;
            outw(io_base + RTL_REG_CAPR, (uint16_t)capr_value);
            rtl8139_acknowledge(RTL_ISR_RX_ERR | RTL_ISR_RX_OVERFLOW);
            break;
        }

        uint16_t frame_len = packet_len - 4;
        rtl8139_copy_from_rx_ring(rx_frame, (offset + 4) % RTL_RX_RING_SIZE, frame_len);
        ethernet_receive_frame(rx_frame, frame_len);
        rtl8139_advance_rx_cursor(packet_len);

        __asm__ volatile("" ::: "memory");
    }

    rtl8139_acknowledge(RTL_ISR_RX_OK | RTL_ISR_RX_ERR | RTL_ISR_RX_OVERFLOW);
    if (!(inb(io_base + RTL_REG_COMMAND) & RTL_CMD_RX_BUFFER_EMPTY)) {
        network_packet_received = 1;
    }
}