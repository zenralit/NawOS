#include <stdint.h>
#include <stddef.h>
#include "drivers/screen/screen.h"
#include "kernel/interrupts/idt.h"
#include "drivers/keyboard/keyboard.h"
#include "drivers/ports/ports.h"
#include "drivers/net/network.h"
#include "fs/nawfs.h"
#include "lib/nawstring.h"
#include "drivers/ata/ata.h"
#include "net/net.h"

void dummy_timer_callback() {}
    
void kernel_main() {
    clear_screen();
    print("Welcome in Sarma. \n");
    print("print command >>>>\n");
    idt_init();
    keyboard_init();
    fs_init();
    network_init();
    asm volatile("sti");
    net_init();

    while (1) {
        
        network_poll();

        if (network_packet_received) {
            network_handle_receive();
        }
        
        net_periodic();
        asm volatile("pause");
    }
}
