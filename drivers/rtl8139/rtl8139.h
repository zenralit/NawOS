#ifndef RTL8139_H
#define RTL8139_H

#include <stdint.h>

int rtl8139_init();
int rtl8139_is_ready();
void rtl8139_poll();
void rtl8139_handle_irq();
int rtl8139_send_packet(const void* data, uint32_t length);
extern uint8_t rtl_mac[6]; 
void rtl8139_handle_receive();

#endif
