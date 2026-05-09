#include "net.h"
#include "dhcp.h"
#include "ip.h"
#include "rtl8139.h"
#include "drivers/screen/screen.h"
#include "lib/nawstring.h"
#include <string.h>

#define NET_TEXT_PORT 5000
#define NET_DHCP_RETRY_TICKS 100000

static const uint8_t broadcast_ip[4] = {255, 255, 255, 255};
static const uint8_t broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static int dhcp_retry_ticks = 0;

net_info_t net_info;
uint8_t naw_mac_address[6];

static uint16_t read_be16(const uint8_t* value) {
    return ((uint16_t)value[0] << 8) | value[1];
}

static void write_be16(uint8_t* dest, uint16_t value) {
    dest[0] = (uint8_t)(value >> 8);
    dest[1] = (uint8_t)(value & 0xFF);
}

static uint16_t ip_checksum(const uint8_t* data, size_t len) {
    uint32_t sum = 0;

    for (size_t i = 0; i < len; i += 2) {
        uint16_t word = (uint16_t)data[i] << 8;
        if (i + 1 < len) {
            word |= data[i + 1];
        }
        sum += word;
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return (uint16_t)(~sum);
}

static void print_ipv4(const uint8_t ip[4]) {
    for (int i = 0; i < 4; i++) {
        print_dec(ip[i]);
        if (i < 3) {
            print(".");
        }
    }
}

static void print_udp_text_payload(const uint8_t* payload, size_t len) {
    size_t limit = len;

    if (limit > 120) {
        limit = 120;
    }

    for (size_t i = 0; i < limit; i++) {
        char c = (char)payload[i];
        if (c >= 32 && c <= 126) {
            put_char(c);
        } else {
            put_char('.');
        }
    }

    if (limit < len) {
       // print("...");
    }
}

void net_init() {
    memset(&net_info, 0, sizeof(net_info));
    memset(naw_ip_address, 0, 4);
    memcpy(naw_mac_address, rtl_mac, sizeof(naw_mac_address));
    memcpy(net_info.mac, rtl_mac, sizeof(net_info.mac));

    dhcp_init();
    net_request_dhcp();
}

void net_periodic() {
    if (net_is_configured()) {
        return;
    }

    if (dhcp_retry_ticks > 0) {
        dhcp_retry_ticks--;
        return;
    }

    dhcp_send_discover();
    dhcp_retry_ticks = NET_DHCP_RETRY_TICKS;
}

int net_is_configured() {
    return net_info.configured;
}

void net_request_dhcp() {
    memset(naw_ip_address, 0, 4);
    memset(net_info.ip, 0, sizeof(net_info.ip));
    memset(net_info.gateway, 0, sizeof(net_info.gateway));
    memset(net_info.subnet, 0, sizeof(net_info.subnet));
    memset(net_info.dhcp_server, 0, sizeof(net_info.dhcp_server));
    net_info.configured = 0;

    //print("NET: requesting DHCP lease\n");
    dhcp_send_discover();
    dhcp_retry_ticks = NET_DHCP_RETRY_TICKS;
}

void net_send_udp_packet(const uint8_t src_ip[4], const uint8_t dest_ip[4], uint16_t src_port, uint16_t dst_port, const uint8_t* data, size_t len) {
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
    write_be16(ip + 2, total_len);
    write_be16(ip + 4, 0);
    write_be16(ip + 6, 0);
    ip[8] = 64;
    ip[9] = 17;
    memcpy(ip + 12, src_ip, 4);
    memcpy(ip + 16, dest_ip, 4);
    write_be16(ip + 10, ip_checksum(ip, 20));

    udp = ip + 20;
    write_be16(udp + 0, src_port);
    write_be16(udp + 2, dst_port);
    udp_len = (uint16_t)(8 + len);
    write_be16(udp + 4, udp_len);
    write_be16(udp + 6, 0);

    if (len > 0) {
        memcpy(udp + 8, data, len);
    }

    rtl8139_send_packet(packet, 14 + total_len);
}

void net_send_text_broadcast(const char* text) {
    if (!net_is_configured()) {
        //print("NET: no IP address yet\n");
        return;
    }

    net_send_udp_packet(naw_ip_address, broadcast_ip, NET_TEXT_PORT, NET_TEXT_PORT, (const uint8_t*)text, strlen(text));
}

void net_receive_frame(const uint8_t* frame, size_t len) {
    const uint8_t* ip;
    const uint8_t* udp;
    const uint8_t* payload;
    uint16_t ethertype;
    uint16_t total_len;
    uint16_t udp_len;
    uint16_t src_port;
    uint16_t dst_port;
    uint8_t ihl;

    if (len < 14) {
        return;
    }

    ethertype = read_be16(frame + 12);
    if (ethertype != 0x0800) {
        return;
    }

    ip = frame + 14;
    if ((size_t)(len - 14) < 20 || (ip[0] >> 4) != 4) {
        return;
    }

    ihl = (uint8_t)((ip[0] & 0x0F) * 4);
    if (ihl < 20 || len < 14 + ihl) {
        return;
    }

    total_len = read_be16(ip + 2);
    if (total_len < ihl) {
        return;
    }
    if (14 + total_len > len) {
        total_len = (uint16_t)(len - 14);
    }

    if (ip[9] != 17 || total_len < ihl + 8) {
        return;
    }

    udp = ip + ihl;
    udp_len = read_be16(udp + 4);
    if (udp_len < 8 || ihl + udp_len > total_len) {
        udp_len = (uint16_t)(total_len - ihl);
    }

    src_port = read_be16(udp + 0);
    dst_port = read_be16(udp + 2);
    payload = udp + 8;

    if (src_port == 67 && dst_port == 68) {
        dhcp_handle_udp_packet(payload, udp_len - 8);
        return;
    }

    if (dst_port == NET_TEXT_PORT) {
        print("\nUDP ");
        print_ipv4(ip + 12);
        print(": ");
        print_udp_text_payload(payload, udp_len - 8);
        print("\n> ");
    }
}
