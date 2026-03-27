#include "dhcp.h"
#include "drivers/screen/screen.h"
#include "ip.h"
#include <stddef.h>
#include "net.h"
#include "rtl8139.h"
#include "lib/nawstring.h"
#include <string.h>

static uint8_t transaction_id = 0x42; 
uint8_t broadcast_ip[4] = { 255, 255, 255, 255 };

void net_send_udp_packet(uint8_t dest_ip[4], uint16_t src_port, uint16_t dst_port, uint8_t* data, size_t len) {
    uint8_t packet[1500];

    memset(packet, 0xFF, 6); 
    memcpy(packet + 6, naw_mac_address, 6);
    packet[12] = 0x08;
    packet[13] = 0x00;


    uint8_t* ip = packet + 14;
    ip[0] = 0x45;
    ip[1] = 0x00;

    uint16_t total_len = 20 + 8 + len;
    ip[2] = total_len >> 8;
    ip[3] = total_len & 0xFF;

    ip[4] = ip[5] = 0;
    ip[6] = ip[7] = 0;
    ip[8] = 64;
    ip[9] = 17; 

    ip[10] = ip[11] = 0;

    memcpy(ip + 12, naw_ip_address, 4);
    memcpy(ip + 16, dest_ip, 4);


    uint32_t sum = 0;
    for (int i = 0; i < 20; i += 2)
        sum += (ip[i] << 8) | ip[i+1];

    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);

    sum = ~sum;
    ip[10] = sum >> 8;
    ip[11] = sum & 0xFF;


    uint8_t* udp = ip + 20;
    udp[0] = src_port >> 8;
    udp[1] = src_port & 0xFF;
    udp[2] = dst_port >> 8;
    udp[3] = dst_port & 0xFF;

    uint16_t udp_len = 8 + len;
    udp[4] = udp_len >> 8;
    udp[5] = udp_len & 0xFF;

    udp[6] = udp[7] = 0;

    memcpy(udp + 8, data, len);

    rtl8139_send_packet(packet, 14 + total_len);
}

void parse_dhcp_offer(uint8_t* packet, size_t size) {
        int base = 14 + 20 + 8 + 16;

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

    packet[0] = 0x01; 
    packet[1] = 0x01; 
    packet[2] = 0x06;
    packet[3] = 0x00; 
    packet[4] = 0x00; packet[5] = 0x00; packet[6] = 0x00; packet[7] = transaction_id; // xid
    packet[8] = packet[9] = packet[10] = packet[11] = 0x00; // seconds + flags
    packet[20] = packet[21] = packet[22] = packet[23] = 0x00; 
    packet[12] = packet[13] = packet[14] = packet[15] = 0x00; 
    packet[16] = packet[17] = packet[18] = packet[19] = 0x00; 
    packet[24] = packet[25] = packet[26] = packet[27] = 0x00; 


    for (int i = 0; i < 6; i++) {
        packet[28 + i] = naw_mac_address[i];
    }

    
    packet[236] = 0x63;
    packet[237] = 0x82;
    packet[238] = 0x53;
    packet[239] = 0x63;

   
    int i = 240;
    packet[i++] = 53; 
    packet[i++] = 1;
    packet[i++] = 1;  

    packet[i++] = 55; // Parameter Request List
    packet[i++] = 3;
    packet[i++] = 1;  // subnet mask
    packet[i++] = 3;  // router
    packet[i++] = 6;  // DNS

    packet[i++] = 255; 

   
    net_send_udp_packet(
        broadcast_ip,    
        68,               
        67,              
        packet,
        i
    );
}
