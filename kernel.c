#include <stdint.h>
#include <stddef.h>
#include "screen.h"
#include "idt.h"
#include "keyboard.h"
#include "ports.h"
#include "nawfs.h"
#include "nawstring.h"
#include "disk.h"
#include "net.h"
#include "rtl8139.h" 
#include "dhcp.h"

extern volatile int net_packet_received;
void dummy_timer_callback() {}
    
void kernel_main() {
    
    keyboard_handle_interrupt();
    clear_screen();
    print("Welcome in NawOS. \n");
    print("print command >>>>\n");
    idt_init();
    keyboard_init();
    fs_init();
    rtl8139_init();
    net_init();
    asm volatile("sti");
    dhcp_send_discover();
    

while (1) {
        asm volatile("hlt");

        if (net_packet_received) {
            rtl8139_handle_receive();
            net_packet_received = 0;
        }

    }
}
