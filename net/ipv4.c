#include "net/ipv4.h"
#include "net/net_limits.h"
#include "net/udp.h"

uint16_t ipv4_read_be16(const uint8_t* value) {
    return ((uint16_t)value[0] << 8) | value[1];
}

void ipv4_write_be16(uint8_t* dest, uint16_t value) {
    dest[0] = (uint8_t)(value >> 8);
    dest[1] = (uint8_t)(value & 0xFF);
}

uint16_t ipv4_checksum(const uint8_t* data, size_t len) {
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

void ipv4_receive_packet(const uint8_t* packet, size_t len) {
    uint16_t total_len;
    uint16_t fragment;
    uint8_t ihl;

    if (!packet || len < NET_IPV4_HEADER_SIZE || (packet[0] >> 4) != 4) {
        return;
    }

    ihl = (uint8_t)((packet[0] & 0x0F) * 4);
    if (ihl < NET_IPV4_HEADER_SIZE || len < ihl) {
        return;
    }

    total_len = ipv4_read_be16(packet + 2);
    if (total_len < ihl || total_len > len ||
        total_len > NET_ETHERNET_MTU) {
        return;
    }

    if (ipv4_checksum(packet, ihl) != 0) {
        return;
    }

    fragment = ipv4_read_be16(packet + 6);
    if ((fragment & 0x3FFF) != 0) {
        return;
    }

    if (packet[9] != 17 || total_len < ihl + NET_UDP_HEADER_SIZE) {
        return;
    }

    udp_receive_packet(packet + 12, packet + 16, packet + ihl, total_len - ihl);
}
