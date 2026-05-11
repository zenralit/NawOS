#include "net/dhcp.h"
#include "kernel/memory/memory.h"
#include "kernel/terminal/terminal.h"
#include "net/ip.h"
#include "net/net.h"
#include "net/udp.h"

#define DHCP_CLIENT_PORT 68
#define DHCP_SERVER_PORT 67
#define DHCP_FIXED_HEADER_SIZE 240

static const uint8_t zero_ip[4] = {0, 0, 0, 0};
static const uint8_t broadcast_ip[4] = {255, 255, 255, 255};
static const uint8_t dhcp_transaction_id[4] = {0x4E, 0x41, 0x57, 0x01};

static uint8_t offered_ip[4];
static uint8_t server_id[4];
static int dhcp_bound = 0;

static void print_ipv4(const uint8_t ip[4]) {
    for (int i = 0; i < 4; i++) {
            terminal_write_dec(ip[i]);
            if (i < 3) {
                terminal_write(".");
            }
        }
}

static int is_same_mac(const uint8_t* packet) {
    return memcmp(packet + 28, naw_mac_address, 6) == 0;
}

static void dhcp_write_header(uint8_t* packet) {
    memset(packet, 0, 548);

    packet[0] = 0x01;
    packet[1] = 0x01;
    packet[2] = 0x06;
    packet[3] = 0x00;
    memcpy(packet + 4, dhcp_transaction_id, sizeof(dhcp_transaction_id));
    packet[10] = 0x80;
    packet[11] = 0x00;
    memcpy(packet + 28, naw_mac_address, 6);

    packet[236] = 0x63;
    packet[237] = 0x82;
    packet[238] = 0x53;
    packet[239] = 0x63;
}

static const uint8_t* dhcp_find_option(const uint8_t* options, size_t options_len, uint8_t code, uint8_t* option_len) {
    size_t offset = 0;

    while (offset < options_len) {
        uint8_t option_code = options[offset++];

        if (option_code == 0) {
            continue;
        }

        if (option_code == 255) {
            break;
        }

        if (offset >= options_len) {
            break;
        }

        uint8_t len = options[offset++];
        if (offset + len > options_len) {
            break;
        }

        if (option_code == code) {
            *option_len = len;
            return options + offset;
        }

        offset += len;
    }

    return 0;
}

static void dhcp_send_request() {
    uint8_t packet[548];
    int index = DHCP_FIXED_HEADER_SIZE;

    dhcp_write_header(packet);

    packet[index++] = 53;
    packet[index++] = 1;
    packet[index++] = 3;

    packet[index++] = 50;
    packet[index++] = 4;
    memcpy(packet + index, offered_ip, 4);
    index += 4;

    packet[index++] = 54;
    packet[index++] = 4;
    memcpy(packet + index, server_id, 4);
    index += 4;

    packet[index++] = 55;
    packet[index++] = 4;
    packet[index++] = 1;
    packet[index++] = 3;
    packet[index++] = 6;
    packet[index++] = 54;
    packet[index++] = 255;

    udp_send_packet(zero_ip, broadcast_ip, DHCP_CLIENT_PORT, DHCP_SERVER_PORT, packet, (size_t)index);
}

void dhcp_init() {
    memset(offered_ip, 0, sizeof(offered_ip));
    memset(server_id, 0, sizeof(server_id));
    dhcp_bound = 0;
}

int dhcp_is_configured() {
    return dhcp_bound;
}

void dhcp_send_discover() {
    uint8_t packet[548];
    int index = DHCP_FIXED_HEADER_SIZE;

    dhcp_bound = 0;
    dhcp_write_header(packet);

    packet[index++] = 53;
    packet[index++] = 1;
    packet[index++] = 1;

    packet[index++] = 55;
    packet[index++] = 4;
    packet[index++] = 1;
    packet[index++] = 3;
    packet[index++] = 6;
    packet[index++] = 54;
    packet[index++] = 255;

    udp_send_packet(zero_ip, broadcast_ip, DHCP_CLIENT_PORT, DHCP_SERVER_PORT, packet, (size_t)index);
}

void dhcp_handle_udp_packet(const uint8_t* packet, size_t size) {
    const uint8_t* options;
    const uint8_t* message_type;
    const uint8_t* server_option;
    const uint8_t* subnet_option;
    const uint8_t* router_option;
    uint8_t option_len = 0;

    if (size < DHCP_FIXED_HEADER_SIZE) {
        return;
    }

    if (packet[0] != 0x02 || packet[1] != 0x01 || packet[2] != 0x06) {
        return;
    }

    if (memcmp(packet + 4, dhcp_transaction_id, sizeof(dhcp_transaction_id)) != 0) {
        return;
    }

    if (!is_same_mac(packet)) {
        return;
    }

    options = packet + DHCP_FIXED_HEADER_SIZE;
    message_type = dhcp_find_option(options, size - DHCP_FIXED_HEADER_SIZE, 53, &option_len);
    if (!message_type || option_len != 1) {
        return;
    }

    if (message_type[0] == 2) {
        server_option = dhcp_find_option(options, size - DHCP_FIXED_HEADER_SIZE, 54, &option_len);
        if (!server_option || option_len != 4) {
            return;
        }

        memcpy(offered_ip, packet + 16, 4);
        memcpy(server_id, server_option, 4);

        terminal_write("DHCPOFFER ");
        print_ipv4(offered_ip);
        terminal_write("\n");

        dhcp_send_request();
        return;
    }

    if (message_type[0] == 5) {
        memcpy(naw_ip_address, packet + 16, 4);
        memcpy(net_info.ip, packet + 16, 4);

        subnet_option = dhcp_find_option(options, size - DHCP_FIXED_HEADER_SIZE, 1, &option_len);
        if (subnet_option && option_len == 4) {
            memcpy(net_info.subnet, subnet_option, 4);
        }

        router_option = dhcp_find_option(options, size - DHCP_FIXED_HEADER_SIZE, 3, &option_len);
        if (router_option && option_len >= 4) {
            memcpy(net_info.gateway, router_option, 4);
        }

        server_option = dhcp_find_option(options, size - DHCP_FIXED_HEADER_SIZE, 54, &option_len);
        if (server_option && option_len == 4) {
            memcpy(net_info.dhcp_server, server_option, 4);
        } else {
            memcpy(net_info.dhcp_server, server_id, 4);
        }

        net_info.configured = 1;
        dhcp_bound = 1;

        terminal_write("DHCPACK IP ");
        print_ipv4(naw_ip_address);
        terminal_write(" GW ");
        print_ipv4(net_info.gateway);
        terminal_write("\n");
        return;
    }

    if (message_type[0] == 6) {
        terminal_write("DHCPNAK received\n");
        dhcp_bound = 0;
        net_info.configured = 0;
    }
}
