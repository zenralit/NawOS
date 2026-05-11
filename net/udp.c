#include "net/udp.h"
#include "drivers/rtl8139/rtl8139.h"
#include "kernel/memory/memory.h"
#include "kernel/terminal/terminal.h"
#include "lib/nawstring.h"
#include "net/dhcp.h"
#include "net/ipv4.h"
#include "net/net.h"

static const uint8_t broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

static void udp_print_ipv4(const uint8_t ip[4]) {
    for (int i = 0; i < 4; i++) {
        terminal_write_dec(ip[i]);
        if (i < 3) {
            terminal_write(".");
        }
    }
}

static void udp_print_text_payload(const uint8_t* payload, size_t len) {
    size_t limit = len;

    if (limit > 120) {
        limit = 120;
    }

    for (size_t i = 0; i < limit; i++) {
        char c = (char)payload[i];
        if (c >= 32 && c <= 126) {
            terminal_put_char(c);
        } else {
            terminal_put_char('.');
        }
    }
}

void udp_send_packet(const uint8_t src_ip[4], const uint8_t dest_ip[4], uint16_t src_port, uint16_t dst_port, const uint8_t* data, size_t len) {
    uint8_t packet[1514];
    uint8_t* ip;
    uint8_t* udp;
    uint16_t total_len;
    uint16_t udp_len;

    if (!rtl8139_is_ready() || len > 1472) {
        return;
    }

    memset(packet, 0, sizeof(packet));
    memcpy(packet, broadcast_mac, 6);
    memcpy(packet + 6, naw_mac_address, 6);
    packet[12] = 0x08;
    packet[13] = 0x00;

    ip = packet + 14;
    ip[0] = 0x45;
    ip[1] = 0x00;
    total_len = (uint16_t)(20 + 8 + len);
    ipv4_write_be16(ip + 2, total_len);
    ipv4_write_be16(ip + 4, 0);
    ipv4_write_be16(ip + 6, 0);
    ip[8] = 64;
    ip[9] = 17;
    memcpy(ip + 12, src_ip, 4);
    memcpy(ip + 16, dest_ip, 4);
    ipv4_write_be16(ip + 10, ipv4_checksum(ip, 20));

    udp = ip + 20;
    ipv4_write_be16(udp + 0, src_port);
    ipv4_write_be16(udp + 2, dst_port);
    udp_len = (uint16_t)(8 + len);
    ipv4_write_be16(udp + 4, udp_len);
    ipv4_write_be16(udp + 6, 0);

    if (len > 0) {
        memcpy(udp + 8, data, len);
    }

    rtl8139_send_packet(packet, 14 + total_len);
}

void udp_receive_packet(const uint8_t src_ip[4], const uint8_t dest_ip[4], const uint8_t* packet, size_t len) {
    const uint8_t* payload;
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t udp_len;

    (void)dest_ip;

    if (len < 8) {
        return;
    }

    udp_len = ipv4_read_be16(packet + 4);
    if (udp_len < 8 || udp_len > len) {
        udp_len = (uint16_t)len;
    }

    src_port = ipv4_read_be16(packet + 0);
    dst_port = ipv4_read_be16(packet + 2);
    payload = packet + 8;

    if (src_port == 67 && dst_port == 68) {
        dhcp_handle_udp_packet(payload, udp_len - 8);
        return;
    }

    if (dst_port == NET_TEXT_PORT) {
        terminal_write("\nUDP ");
        udp_print_ipv4(src_ip);
        terminal_write(": ");
        udp_print_text_payload(payload, udp_len - 8);
        terminal_write("\n> ");
    }
}
