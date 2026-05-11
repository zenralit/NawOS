#ifndef NET_UDP_H
#define NET_UDP_H

#include <stddef.h>
#include <stdint.h>

#define NET_TEXT_PORT 5000

void udp_send_packet(const uint8_t src_ip[4], const uint8_t dest_ip[4], uint16_t src_port, uint16_t dst_port, const uint8_t* data, size_t len);
void udp_receive_packet(const uint8_t src_ip[4], const uint8_t dest_ip[4], const uint8_t* packet, size_t len);

#endif
