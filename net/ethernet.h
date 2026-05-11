#ifndef NET_ETHERNET_H
#define NET_ETHERNET_H

#include <stddef.h>
#include <stdint.h>

void ethernet_receive_frame(const uint8_t* frame, size_t len);

#endif
