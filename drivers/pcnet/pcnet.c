#include "drivers/pcnet/pcnet.h"
#include "drivers/pci/pci.h"
#include "drivers/ports/ports.h"
#include "drivers/net/network.h"
#include "kernel/memory/memory.h"
#include "kernel/terminal/terminal.h"
#include "net/ethernet.h"
#include "net/net_limits.h"

#define PCNET_VENDOR_ID 0x1022
#define PCNET_DEVICE_ID 0x2000

#define PCNET_RX_RING_LOG2 4
#define PCNET_TX_RING_LOG2 2
#define PCNET_RX_RING_SIZE (1U << PCNET_RX_RING_LOG2)
#define PCNET_TX_RING_SIZE (1U << PCNET_TX_RING_LOG2)
#define PCNET_MAX_FRAME_SIZE NET_ETHERNET_FRAME_MAX_SIZE
#define PCNET_RX_BUFFER_SIZE NET_ETHERNET_FRAME_WITH_FCS_MAX_SIZE

#define PCNET_REG_RDP 0x10
#define PCNET_REG_RAP 0x12
#define PCNET_REG_RESET 0x14
#define PCNET_REG_BDP 0x16

#define PCNET_CSR0 0
#define PCNET_CSR0_INIT 0x0001
#define PCNET_CSR0_START 0x0002
#define PCNET_CSR0_STOP 0x0004
#define PCNET_CSR0_INTEN 0x0040
#define PCNET_CSR0_IDON 0x0100
#define PCNET_CSR0_TDMD 0x0008
#define PCNET_CSR0_RINT 0x0400
#define PCNET_CSR0_ACK_MASK 0xFE00

#define PCNET_BCR20 20
#define PCNET_CSR4 4

#define PCNET_DESC_OWN 0x8000
#define PCNET_DESC_ERR 0x4000
#define PCNET_DESC_STP 0x0200
#define PCNET_DESC_ENP 0x0100

typedef struct {
    uint32_t base;
    uint16_t buffer_length;
    uint16_t status;
    uint32_t message_length;
    uint32_t reserved;
} __attribute__((packed, aligned(16))) pcnet_descriptor_t;

typedef struct {
    uint16_t mode;
    uint16_t transmit_receive_length;
    uint8_t physical_address[6];
    uint16_t reserved;
    uint32_t logical_filter[2];
    uint32_t receive_ring;
    uint32_t transmit_ring;
} __attribute__((packed, aligned(16))) pcnet_init_block_t;

static uint32_t io_base;
static int pcnet_ready;
static uint8_t receive_index;
static uint8_t transmit_index;

uint8_t pcnet_mac[6];

static volatile pcnet_descriptor_t receive_ring[PCNET_RX_RING_SIZE] __attribute__((aligned(16)));
static volatile pcnet_descriptor_t transmit_ring[PCNET_TX_RING_SIZE] __attribute__((aligned(16)));
static pcnet_init_block_t init_block __attribute__((aligned(16)));
static uint8_t receive_buffers[PCNET_RX_RING_SIZE][PCNET_RX_BUFFER_SIZE] __attribute__((aligned(16)));
static uint8_t transmit_buffers[PCNET_TX_RING_SIZE][PCNET_MAX_FRAME_SIZE] __attribute__((aligned(16)));

static uint16_t pcnet_read_csr(uint16_t index) {
    outw(io_base + PCNET_REG_RAP, index);
    return inw(io_base + PCNET_REG_RDP);
}

static void pcnet_write_csr(uint16_t index, uint16_t value) {
    outw(io_base + PCNET_REG_RAP, index);
    outw(io_base + PCNET_REG_RDP, value);
}

static void pcnet_write_bcr(uint16_t index, uint16_t value) {
    outw(io_base + PCNET_REG_RAP, index);
    outw(io_base + PCNET_REG_BDP, value);
}

static void pcnet_acknowledge(uint16_t status) {
    uint16_t pending = status & PCNET_CSR0_ACK_MASK;
    if (pending) {
        pcnet_write_csr(PCNET_CSR0, pending);
    }
}

int pcnet_is_ready() {
    return pcnet_ready;
}

int pcnet_init() {
    pci_device_t dev = pci_find_device(PCNET_VENDOR_ID, PCNET_DEVICE_ID);
    uint32_t bar;

    pcnet_ready = 0;
    if (dev.vendor_id == 0xFFFF) {
        return 0;
    }

    pci_enable_bus_mastering(dev.bus, dev.slot, dev.func);
    bar = pci_get_bar(dev.bus, dev.slot, dev.func, 0);
    if ((bar & 1) == 0) {
        terminal_write("PCnet has an invalid I/O BAR.\n");
        return 0;
    }

    io_base = bar & ~0x3U;
    if (io_base == 0 || io_base == 0xFFFFFFFC) {
        terminal_write("PCnet has an invalid I/O BAR.\n");
        return 0;
    }

    inw(io_base + PCNET_REG_RESET);
    pcnet_write_csr(PCNET_CSR0, PCNET_CSR0_STOP);
    pcnet_write_bcr(PCNET_BCR20, 2);

    receive_index = 0;
    transmit_index = 0;
    memset(receive_buffers, 0, sizeof(receive_buffers));
    memset(transmit_buffers, 0, sizeof(transmit_buffers));
    memset(&init_block, 0, sizeof(init_block));

    for (uint8_t i = 0; i < PCNET_RX_RING_SIZE; i++) {
        receive_ring[i].base = (uint32_t)receive_buffers[i];
        receive_ring[i].buffer_length = (uint16_t)(-(int)PCNET_RX_BUFFER_SIZE);
        receive_ring[i].message_length = 0;
        receive_ring[i].reserved = 0;
        receive_ring[i].status = PCNET_DESC_OWN;
    }

    for (uint8_t i = 0; i < PCNET_TX_RING_SIZE; i++) {
        transmit_ring[i].base = 0;
        transmit_ring[i].buffer_length = 0;
        transmit_ring[i].status = 0;
        transmit_ring[i].message_length = 0;
        transmit_ring[i].reserved = 0;
    }

    for (uint8_t i = 0; i < 6; i++) {
        pcnet_mac[i] = inb(io_base + i);
        init_block.physical_address[i] = pcnet_mac[i];
    }

    init_block.transmit_receive_length =
        (uint16_t)((PCNET_TX_RING_LOG2 << 12) | (PCNET_RX_RING_LOG2 << 4));
    init_block.receive_ring = (uint32_t)receive_ring;
    init_block.transmit_ring = (uint32_t)transmit_ring;

    __asm__ volatile("" ::: "memory");
    pcnet_write_csr(1, (uint16_t)(uint32_t)&init_block);
    pcnet_write_csr(2, (uint16_t)((uint32_t)&init_block >> 16));
    pcnet_write_csr(PCNET_CSR4, 0x0915);
    pcnet_write_csr(PCNET_CSR0, PCNET_CSR0_INIT);

    uint32_t timeout = 10000000;
    while (!(pcnet_read_csr(PCNET_CSR0) & PCNET_CSR0_IDON) && timeout > 0) {
        __asm__ volatile("pause");
        timeout--;
    }

    if (timeout == 0) {
        pcnet_write_csr(PCNET_CSR0, PCNET_CSR0_STOP);
        terminal_write("PCnet initialization timed out.\n");
        return 0;
    }

    pcnet_write_csr(PCNET_CSR0, PCNET_CSR0_START | PCNET_CSR0_INTEN);
    pcnet_ready = 1;

    terminal_write("PCnet MAC: ");
    for (uint8_t i = 0; i < 6; i++) {
        terminal_write_hex_byte(pcnet_mac[i]);
        if (i < 5) {
            terminal_write(":");
        }
    }
    terminal_write("\n");
    return 1;
}

void pcnet_poll() {
    if (!pcnet_ready) {
        return;
    }

    if (!(receive_ring[receive_index].status & PCNET_DESC_OWN)) {
        network_set_packet_flag();
    }

    uint16_t status = pcnet_read_csr(PCNET_CSR0);
    pcnet_acknowledge(status);
}

void pcnet_handle_irq() {
    if (!pcnet_ready) {
        return;
    }

    uint16_t status = pcnet_read_csr(PCNET_CSR0);
    if (status & PCNET_CSR0_RINT) {
        network_set_packet_flag();
    }
    pcnet_acknowledge(status);
}

int pcnet_send_packet(const void* data, uint32_t length) {
    if (!pcnet_ready || !data || length == 0 || length > PCNET_MAX_FRAME_SIZE) {
        return 0;
    }

    volatile pcnet_descriptor_t* descriptor = &transmit_ring[transmit_index];
    if (descriptor->status & PCNET_DESC_OWN) {
        return 0;
    }

    memcpy(transmit_buffers[transmit_index], data, length);
    descriptor->base = (uint32_t)transmit_buffers[transmit_index];
    descriptor->buffer_length = (uint16_t)(-(int)length);
    descriptor->message_length = 0;
    descriptor->reserved = 0;
    __asm__ volatile("" ::: "memory");
    descriptor->status = PCNET_DESC_OWN | PCNET_DESC_STP | PCNET_DESC_ENP;

    transmit_index = (transmit_index + 1) % PCNET_TX_RING_SIZE;
    pcnet_write_csr(PCNET_CSR0, PCNET_CSR0_START | PCNET_CSR0_TDMD);
    return 1;
}

void pcnet_handle_receive() {
    if (!pcnet_ready) {
        return;
    }

    network_packet_received = 0;
    for (uint8_t processed = 0; processed < PCNET_RX_RING_SIZE; processed++) {
        volatile pcnet_descriptor_t* descriptor = &receive_ring[receive_index];
        uint16_t status = descriptor->status;
        if (status & PCNET_DESC_OWN) {
            break;
        }

        uint32_t frame_length = descriptor->message_length & 0x0FFF;
        if (!(status & PCNET_DESC_ERR) &&
            (status & (PCNET_DESC_STP | PCNET_DESC_ENP)) ==
                (PCNET_DESC_STP | PCNET_DESC_ENP) &&
            frame_length >= 4 && frame_length <= PCNET_RX_BUFFER_SIZE) {
            ethernet_receive_frame(receive_buffers[receive_index], frame_length - 4);
        }

        descriptor->message_length = 0;
        descriptor->buffer_length = (uint16_t)(-(int)PCNET_RX_BUFFER_SIZE);
        __asm__ volatile("" ::: "memory");
        descriptor->status = PCNET_DESC_OWN;
        receive_index = (receive_index + 1) % PCNET_RX_RING_SIZE;
    }

    pcnet_acknowledge(pcnet_read_csr(PCNET_CSR0));
    if (!(receive_ring[receive_index].status & PCNET_DESC_OWN)) {
        network_packet_received = 1;
    }
}
