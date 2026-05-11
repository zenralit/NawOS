#ifndef DHCP_H
#define DHCP_H

#include <stddef.h>
#include <stdint.h>

void dhcp_init();
void dhcp_send_discover();
void dhcp_handle_udp_packet(const uint8_t* packet, size_t size);
int dhcp_is_configured();

#endif
