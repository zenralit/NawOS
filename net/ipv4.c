#include "net/ipv4.h"
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
    uint8_t ihl;

    if (len < 20 || (packet[0] >> 4) != 4) {
        return;
    }

    ihl = (uint8_t)((packet[0] & 0x0F) * 4);
    if (ihl < 20 || len < ihl) {
        return;
    }

    total_len = ipv4_read_be16(packet + 2);
    if (total_len < ihl) {
        return;
    }

    if (total_len > len) {
        total_len = (uint16_t)len;
    }

    if (packet[9] != 17 || total_len < ihl + 8) {
        return;
    }

    udp_receive_packet(packet + 12, packet + 16, packet + ihl, total_len - ihl);
}
