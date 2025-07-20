#include <stddef.h>
#include <stdint.h>
#ifndef DHCP_H
#define DHCP_H


void dhcp_send_discover();

void parse_dhcp_offer(uint8_t* packet, size_t size);

#endif
