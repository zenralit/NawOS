#include "net/net.h"
#include "drivers/rtl8139/rtl8139.h"
#include "kernel/memory/memory.h"
#include "lib/nawstring.h"
#include "net/dhcp.h"
#include "net/ip.h"
#include "net/udp.h"

#define NET_DHCP_RETRY_TICKS 100000

static const uint8_t broadcast_ip[4] = {255, 255, 255, 255};
static int dhcp_retry_ticks = 0;

net_info_t net_info;
uint8_t naw_mac_address[6];

void net_init() {
    memset(&net_info, 0, sizeof(net_info));
    memset(naw_ip_address, 0, 4);
    memcpy(naw_mac_address, rtl_mac, sizeof(naw_mac_address));
    memcpy(net_info.mac, rtl_mac, sizeof(net_info.mac));

    dhcp_init();
    net_request_dhcp();
}

void net_periodic() {
    if (net_is_configured()) {
        return;
    }

    if (dhcp_retry_ticks > 0) {
        dhcp_retry_ticks--;
        return;
    }

    dhcp_send_discover();
    dhcp_retry_ticks = NET_DHCP_RETRY_TICKS;
}

int net_is_configured() {
    return net_info.configured;
}

void net_request_dhcp() {
    memset(naw_ip_address, 0, 4);
    memset(net_info.ip, 0, sizeof(net_info.ip));
    memset(net_info.gateway, 0, sizeof(net_info.gateway));
    memset(net_info.subnet, 0, sizeof(net_info.subnet));
    memset(net_info.dhcp_server, 0, sizeof(net_info.dhcp_server));
    net_info.configured = 0;

    //print("NET: requesting DHCP lease\n");
    dhcp_send_discover();
    dhcp_retry_ticks = NET_DHCP_RETRY_TICKS;
}

void net_send_text_broadcast(const char* text) {
    if (!net_is_configured()) {
        //print("NET: no IP address yet\n");
        return;
    }

    udp_send_packet(naw_ip_address, broadcast_ip, NET_TEXT_PORT, NET_TEXT_PORT, (const uint8_t*)text, strlen(text));
}
