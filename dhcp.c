#include "dhcp.h"
#include "screen.h"
#include "ip.h"
#include <stddef.h>
#include "net.h"
#include "rtl8139.h"
#include "nawstring.h"
#include <string.h>

static uint8_t transaction_id = 0x42; 
uint8_t broadcast_ip[4] = { 255, 255, 255, 255 };

void net_send_udp_packet(uint8_t dest_ip[4], uint16_t src_port, uint16_t dst_port, uint8_t* data, size_t len) {
   
}

void parse_dhcp_offer(uint8_t* packet, size_t size) {
    // if (!(packet[236] == 0x63 && packet[237] == 0x82 &&
    //       packet[238] == 0x53 && packet[239] == 0x63)) {
    //     print("Not a valid DHCP packet\n");
    //     return;
    // }

    // for (int i = 0; i < 4; i++) {
    //     naw_ip_address[i] = packet[16 + i];
    // }

    // print("Assigned IP: ");
    // for (int i = 0; i < 4; i++) {
    //     print_dec(naw_ip_address[i]);
    //     if (i < 3) print(".");
    // }
    // print("\n");


        int base = 42 + 16;
    for (int i = 0; i < 4; i++) {
        naw_ip_address[i] = packet[base + i];
    }

    print("Assigned IP: ");
    for (int i = 0; i < 4; i++) {
        print_dec(naw_ip_address[i]);
        if (i < 3) print(".");
    }
        print("\n");
}

void dhcp_send_discover() {
    uint8_t packet[548];
    memset(packet, 0, sizeof(packet));

    // Ethernet (MAC + IP) = handled by rtl8139_send_packet
    // BOOTP + DHCP
    packet[0] = 0x01; // BOOTREQUEST
    packet[1] = 0x01; // Ethernet
    packet[2] = 0x06; // MAC len
    packet[3] = 0x00; // hops
    packet[4] = 0x00; packet[5] = 0x00; packet[6] = 0x00; packet[7] = transaction_id; // xid
    packet[8] = packet[9] = packet[10] = packet[11] = 0x00; // seconds + flags
    packet[12] = packet[13] = packet[14] = packet[15] = 0x00; // ciaddr
    packet[16] = packet[17] = packet[18] = packet[19] = 0x00; // yiaddr
    packet[20] = packet[21] = packet[22] = packet[23] = 0x00; // siaddr
    packet[24] = packet[25] = packet[26] = packet[27] = 0x00; // giaddr

    // chaddr (client hardware address)
    for (int i = 0; i < 6; i++) {
        packet[28 + i] = naw_mac_address[i];
    }

    // magic cookie
    packet[236] = 0x63;
    packet[237] = 0x82;
    packet[238] = 0x53;
    packet[239] = 0x63;

    // DHCP options
    int i = 240;
    packet[i++] = 53; // DHCP Message Type
    packet[i++] = 1;
    packet[i++] = 1;  // DHCPDISCOVER

    packet[i++] = 55; // Parameter Request List
    packet[i++] = 3;
    packet[i++] = 1;  // subnet mask
    packet[i++] = 3;  // router
    packet[i++] = 6;  // DNS

    packet[i++] = 255; // end

   
    net_send_udp_packet(
        broadcast_ip,     // 255.255.255.255
        68,               // src port
        67,               // dst port
        packet,
        i
    );
}