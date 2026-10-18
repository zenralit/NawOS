#include "drivers/net/network.h"
#include "drivers/pcnet/pcnet.h"
#include "drivers/rtl8139/rtl8139.h"
#include "kernel/terminal/terminal.h"

typedef enum {
    NETWORK_DEVICE_NONE,
    NETWORK_DEVICE_RTL8139,
    NETWORK_DEVICE_PCNET
} network_device_t;

static network_device_t active_device = NETWORK_DEVICE_NONE;

volatile int network_packet_received = 0;
uint8_t network_mac[6];

void network_set_packet_flag() {
    network_packet_received = 1;
}

void network_init() {
    active_device = NETWORK_DEVICE_NONE;
    network_packet_received = 0;
    for (int i = 0; i < 6; i++) {
        network_mac[i] = 0;
    }

    if (rtl8139_init()) {
        active_device = NETWORK_DEVICE_RTL8139;
        for (int i = 0; i < 6; i++) {
            network_mac[i] = rtl_mac[i];
        }
        return;
    }

    if (pcnet_init()) {
        active_device = NETWORK_DEVICE_PCNET;
        for (int i = 0; i < 6; i++) {
            network_mac[i] = pcnet_mac[i];
        }
        return;
    }

    terminal_write("No supported network adapter found.\n");
}

int network_is_ready() {
    return active_device != NETWORK_DEVICE_NONE;
}

void network_poll() {
    if (active_device == NETWORK_DEVICE_RTL8139) {
        rtl8139_poll();
    } else if (active_device == NETWORK_DEVICE_PCNET) {
        pcnet_poll();
    }
}

void network_handle_irq() {
    if (active_device == NETWORK_DEVICE_RTL8139) {
        rtl8139_handle_irq();
    } else if (active_device == NETWORK_DEVICE_PCNET) {
        pcnet_handle_irq();
    }
}

void network_handle_receive() {
    if (active_device == NETWORK_DEVICE_RTL8139) {
        rtl8139_handle_receive();
    } else if (active_device == NETWORK_DEVICE_PCNET) {
        pcnet_handle_receive();
    } else {
        network_packet_received = 0;
    }
}

int network_send_packet(const void* data, uint32_t length) {
    if (active_device == NETWORK_DEVICE_RTL8139) {
        return rtl8139_send_packet(data, length);
    }
    if (active_device == NETWORK_DEVICE_PCNET) {
        return pcnet_send_packet(data, length);
    }
    return 0;
}
