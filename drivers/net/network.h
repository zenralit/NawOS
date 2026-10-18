#ifndef NETWORK_H
#define NETWORK_H

#include <stdint.h>

extern volatile int network_packet_received;
extern uint8_t network_mac[6];

void network_init();
int network_is_ready();
void network_poll();
void network_handle_irq();
void network_handle_receive();
int network_send_packet(const void* data, uint32_t length);
void network_set_packet_flag();

#endif
