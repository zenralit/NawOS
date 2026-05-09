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
    uint8_t dhcp_server[4];
    int configured;
} net_info_t;

extern net_info_t net_info;

void net_init();
void net_periodic();
int net_is_configured();
void net_request_dhcp();
void net_receive_frame(const uint8_t* frame, size_t len);
void net_send_udp_packet(const uint8_t src_ip[4], const uint8_t dest_ip[4], uint16_t src_port, uint16_t dst_port, const uint8_t* data, size_t len);
void net_send_text_broadcast(const char* text);

#endif
