#include "net/net.h"
#include "drivers/net/network.h"
#include "kernel/memory/memory.h"
#include "kernel/terminal/terminal.h"
#include "lib/nawstring.h"
#include "net/dhcp.h"
#include "net/ip.h"
#include "net/udp.h"

#define NET_DHCP_RETRY_TICKS 100000

static const uint8_t broadcast_ip[4] = {255, 255, 255, 255};
static int dhcp_retry_ticks = 0;
static int static_ip_configured = 0;

net_info_t net_info;
uint8_t naw_mac_address[6];

void net_init() {
    memset(&net_info, 0, sizeof(net_info));
    static_ip_configured = 0;
    memset(naw_ip_address, 0, 4);
    memcpy(naw_mac_address, network_mac, sizeof(naw_mac_address));
    memcpy(net_info.mac, network_mac, sizeof(net_info.mac));

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
    return static_ip_configured || (net_info.configured && dhcp_is_configured());
}

void net_request_dhcp() {
    static_ip_configured = 0;
    memset(naw_ip_address, 0, 4);
    memset(net_info.ip, 0, sizeof(net_info.ip));
    memset(net_info.gateway, 0, sizeof(net_info.gateway));
    memset(net_info.subnet, 0, sizeof(net_info.subnet));
    memset(net_info.dhcp_server, 0, sizeof(net_info.dhcp_server));
    net_info.configured = 0;

    if (!network_is_ready()) {
        terminal_write("DHCP failed: no network adapter is ready\n");
    } else if (!dhcp_send_discover()) {
        terminal_write("DHCP discover could not be queued\n");
    }
    dhcp_retry_ticks = NET_DHCP_RETRY_TICKS;
}

int net_set_static_ip(const uint8_t ip[4]) {
    if (!network_is_ready() || !ip) {
        return 0;
    }

    if ((ip[0] == 0 && ip[1] == 0 && ip[2] == 0 && ip[3] == 0) ||
        (ip[0] == 255 && ip[1] == 255 && ip[2] == 255 && ip[3] == 255)) {
        return 0;
    }

    memcpy(naw_ip_address, ip, sizeof(net_info.ip));
    memcpy(net_info.ip, ip, sizeof(net_info.ip));
    memset(net_info.gateway, 0, sizeof(net_info.gateway));
    memset(net_info.subnet, 0, sizeof(net_info.subnet));
    memset(net_info.dhcp_server, 0, sizeof(net_info.dhcp_server));
    net_info.configured = 1;
    static_ip_configured = 1;
    dhcp_init();
    dhcp_retry_ticks = NET_DHCP_RETRY_TICKS;
    return 1;
}

int net_send_text_broadcast(const char* text) {
    if (!net_is_configured() || !text) {
        return 0;
    }

    return udp_send_packet(naw_ip_address, broadcast_ip, NET_TEXT_PORT,
                           NET_TEXT_PORT, (const uint8_t*)text, strlen(text));
}
