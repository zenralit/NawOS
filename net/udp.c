#include "net/udp.h"
#include "drivers/net/network.h"
#include "kernel/memory/memory.h"
#include "kernel/terminal/terminal.h"
#include "lib/nawstring.h"
#include "net/dhcp.h"
#include "net/ipv4.h"
#include "net/net_limits.h"
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
    for (size_t i = 0; i < len; i++) {
        char c = (char)payload[i];
        if (c >= 32 && c <= 126) {
            terminal_put_char(c);
        } else {
            terminal_put_char('.');
        }
    }
}

int udp_send_packet(const uint8_t src_ip[4], const uint8_t dest_ip[4], uint16_t src_port, uint16_t dst_port, const uint8_t* data, size_t len) {
    uint8_t packet[NET_ETHERNET_FRAME_MAX_SIZE];
    uint8_t* ip;
    uint8_t* udp;
    uint16_t total_len;
    uint16_t udp_len;

    if (!network_is_ready() || !src_ip || !dest_ip ||
        (len > 0 && !data) || len > NET_UDP_PAYLOAD_MAX_SIZE) {
        return 0;
    }

    memset(packet, 0, sizeof(packet));
    memcpy(packet, broadcast_mac, 6);
    memcpy(packet + 6, naw_mac_address, 6);
    packet[12] = 0x08;
    packet[13] = 0x00;

    ip = packet + NET_ETHERNET_HEADER_SIZE;
    ip[0] = 0x45;
    ip[1] = 0x00;
    total_len = (uint16_t)(NET_IPV4_HEADER_SIZE + NET_UDP_HEADER_SIZE + len);
    ipv4_write_be16(ip + 2, total_len);
    ipv4_write_be16(ip + 4, 0);
    ipv4_write_be16(ip + 6, 0);
    ip[8] = 64;
    ip[9] = 17;
    memcpy(ip + 12, src_ip, 4);
    memcpy(ip + 16, dest_ip, 4);
    ipv4_write_be16(ip + 10, ipv4_checksum(ip, NET_IPV4_HEADER_SIZE));

    udp = ip + NET_IPV4_HEADER_SIZE;
    ipv4_write_be16(udp + 0, src_port);
    ipv4_write_be16(udp + 2, dst_port);
    udp_len = (uint16_t)(NET_UDP_HEADER_SIZE + len);
    ipv4_write_be16(udp + 4, udp_len);
    ipv4_write_be16(udp + 6, 0);

    if (len > 0) {
        memcpy(udp + 8, data, len);
    }

    uint32_t frame_len = NET_ETHERNET_HEADER_SIZE + total_len;
    if (frame_len < NET_ETHERNET_MIN_FRAME_SIZE) {
        frame_len = NET_ETHERNET_MIN_FRAME_SIZE;
    }
    return network_send_packet(packet, frame_len);
}

void udp_receive_packet(const uint8_t src_ip[4], const uint8_t dest_ip[4], const uint8_t* packet, size_t len) {
    const uint8_t* payload;
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t udp_len;

    if (!src_ip || !dest_ip || !packet || len < NET_UDP_HEADER_SIZE) {
        return;
    }

    udp_len = ipv4_read_be16(packet + 4);
    if (udp_len < NET_UDP_HEADER_SIZE || udp_len != len) {
        return;
    }

    src_port = ipv4_read_be16(packet + 0);
    dst_port = ipv4_read_be16(packet + 2);
    payload = packet + NET_UDP_HEADER_SIZE;

    if (src_port == 67 && dst_port == 68) {
        dhcp_handle_udp_packet(payload, udp_len - NET_UDP_HEADER_SIZE);
        return;
    }

    if (dst_port == NET_TEXT_PORT) {
        terminal_begin_batch();
        terminal_write("\nfrom ");
        udp_print_ipv4(src_ip);
        terminal_write(" > ");
        udp_print_text_payload(payload, udp_len - NET_UDP_HEADER_SIZE);
        terminal_write("\n> ");
        terminal_end_batch();
    }
}
