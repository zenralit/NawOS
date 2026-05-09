#ifndef RTL8139_H
#define RTL8139_H
#include <stdint.h>

extern volatile int net_packet_received;

void set_net_packet_flag();
void rtl8139_init();
int rtl8139_is_ready();
void rtl8139_poll();
void rtl8139_handle_irq();
void rtl8139_send_packet(void* data, uint32_t length);
extern uint8_t rtl_mac[6]; 
void rtl8139_handle_receive();

#endif
