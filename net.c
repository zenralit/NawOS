#include "net.h"
#include "rtl8139.h"
#include "screen.h"
#include <string.h>

net_info_t net_info;

#define DHCP_CLIENT_PORT 68
#define DHCP_SERVER_PORT 67

uint8_t naw_mac_address[6] = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x01 };

static uint8_t dhcp_discover_packet[300];

static uint8_t transaction_id[] = {0x39, 0x03, 0xF3, 0x26}; 

void net_init() {
    memset(&net_info, 0, sizeof(net_info));
    memcpy(net_info.mac, rtl_mac, 6);
    send_dhcp_discover();
}

void send_dhcp_discover() {
    memset(dhcp_discover_packet, 0, sizeof(dhcp_discover_packet));


    uint8_t* eth = dhcp_discover_packet;
    memset(eth, 0xFF, 6);                      
    memcpy(eth + 6, rtl_mac, 6);               
    eth[12] = 0x08; eth[13] = 0x00;            

  
    uint8_t* ip = eth + 14;
    ip[0] = 0x45;                              
    ip[1] = 0x00;                             
    uint16_t total_len = 20 + 8 + 240;        
    ip[2] = (total_len >> 8); ip[3] = (total_len & 0xFF);
    ip[4] = 0x00; ip[5] = 0x00;
    ip[6] = 0x00; ip[7] = 0x00;
    ip[8] = 0x80;
    ip[9] = 0x11;
    ip[10] = 0; ip[11] = 0;
    memset(ip + 12, 0, 8);
    memset(ip + 16, 0xFF, 4);

    uint32_t sum = 0;
    for (int i = 0; i < 20; i += 2)
        sum += (ip[i] << 8) | ip[i+1];
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    sum = ~sum;
    ip[10] = sum >> 8; ip[11] = sum & 0xFF;


    uint8_t* udp = ip + 20;
    udp[0] = 0x00; udp[1] = DHCP_CLIENT_PORT;
    udp[2] = 0x00; udp[3] = DHCP_SERVER_PORT;
    uint16_t udp_len = 8 + 240;
    udp[4] = (udp_len >> 8); udp[5] = (udp_len & 0xFF);
    udp[6] = 0; udp[7] = 0;

   
    uint8_t* dhcp = udp + 8;
    dhcp[0] = 0x01; 
    dhcp[1] = 0x01; 
    dhcp[2] = 0x06;
    dhcp[3] = 0x00;
    memcpy(dhcp + 4, transaction_id, 4);
    dhcp[8] = 0x00; dhcp[9] = 0x00;
    dhcp[10] = 0x80; dhcp[11] = 0x00;
    memset(dhcp + 12, 0, 16);
    memcpy(dhcp + 28, rtl_mac, 6);
    dhcp[236] = 99; dhcp[237] = 130; dhcp[238] = 83; dhcp[239] = 99;


    uint8_t* opt = dhcp + 240;
    opt[0] = 53; opt[1] = 1; opt[2] = 1;
    opt[3] = 55; opt[4] = 2; opt[5] = 1; opt[6] = 3;
    opt[7] = 255;

    rtl8139_send_packet(dhcp_discover_packet, 14 + 20 + 8 + 240);
    print("Sent DHCPDISCOVER\n");
}
