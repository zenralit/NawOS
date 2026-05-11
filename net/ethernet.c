#include "net/ethernet.h"
#include "net/ipv4.h"

static uint16_t ethernet_read_be16(const uint8_t* value) {
    return ((uint16_t)value[0] << 8) | value[1];
}

void ethernet_receive_frame(const uint8_t* frame, size_t len) {
    uint16_t ethertype;

    if (len < 14) {
        return;
    }

    ethertype = ethernet_read_be16(frame + 12);
    if (ethertype != 0x0800) {
        return;
    }

    ipv4_receive_packet(frame + 14, len - 14);
}
