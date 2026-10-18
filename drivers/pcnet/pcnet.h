#ifndef PCNET_H
#define PCNET_H

#include <stdint.h>

extern uint8_t pcnet_mac[6];

int pcnet_init();
int pcnet_is_ready();
void pcnet_poll();
void pcnet_handle_irq();
void pcnet_handle_receive();
int pcnet_send_packet(const void* data, uint32_t length);

#endif
