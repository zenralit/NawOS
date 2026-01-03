#include <stdint.h>
#include <stddef.h>
#include "drivers/screen/screen.h"
#include "idt.h"
#include "drivers/keyboard/keyboard.h"
#include "drivers/ports/ports.h"
#include "fs/nawfs.h"
#include "lib/nawstring.h"
#include "drivers/disk/disk.h"
#include "drivers/net/net.h"
#include "drivers/net/rtl8139.h" 
#include "drivers/net/dhcp.h"

extern uint8_t naw_ip_address[4];
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
    
    
    int tries = 5;
    while (tries-- && naw_ip_address[0] == 0) {
        dhcp_send_discover();

        for (int i = 0; i < 100; i++) {
            if (net_packet_received) {
                rtl8139_handle_receive();
                net_packet_received = 0;
            }
        }
    }
   
//1.1 что такое ОС. состав ОС
//1.2 какие ОС существуют
//2.1 проектирование / анализ
//2.2 выбор технологий, стек, среда
//2.3 описание nawOS, технологии разработки 
//3.1 разработка ядра
//3.2 модуль фс
//3.3 тестирование

    while (1) {
            asm volatile("hlt");

            if (net_packet_received) {
                rtl8139_handle_receive();
                net_packet_received = 0;
            }

        }
}
