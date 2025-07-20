#ifndef NET_H
#define NET_H

#include <stdint.h>
#include <stddef.h>

extern uint8_t naw_mac_address[6];


typedef struct {
    uint8_t mac[6];
    uint8_t ip[4];
    uint8_t gateway[4];
    uint8_t subnet[4];
} net_info_t;

extern net_info_t net_info;

void net_init();
void send_dhcp_discover();
void handle_dhcp_offer(uint8_t* data, uint32_t len);
void net_send_udp_packet(uint8_t dest_ip[4], uint16_t src_port, uint16_t dst_port, uint8_t* data, size_t len);

#endif
