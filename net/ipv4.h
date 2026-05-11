#ifndef NET_IPV4_H
#define NET_IPV4_H

#include <stddef.h>
#include <stdint.h>

uint16_t ipv4_read_be16(const uint8_t* value);
void ipv4_write_be16(uint8_t* dest, uint16_t value);
uint16_t ipv4_checksum(const uint8_t* data, size_t len);
void ipv4_receive_packet(const uint8_t* packet, size_t len);

#endif
