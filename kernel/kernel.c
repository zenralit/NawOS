#include <stdint.h>
#include <stddef.h>
#include "drivers/screen/screen.h"
#include "kernel/interrupts/idt.h"
#include "drivers/keyboard/keyboard.h"
#include "drivers/ports/ports.h"
#include "fs/nawfs.h"
#include "lib/nawstring.h"
#include "drivers/ata/ata.h"
#include "net/net.h"
#include "drivers/rtl8139/rtl8139.h" 

void dummy_timer_callback() {}
    
void kernel_main() {
    clear_screen();
    print("Welcome in Sarma. \n");
    print("print command >>>>\n");
    idt_init();
    keyboard_init();
    fs_init();
    rtl8139_init();
    /* Прерывания включаем до сети: IRQ клавиатуры и RTL8139 уже замаплены в IDT. */
    asm volatile("sti");
    net_init();

    /*
     * Нет отдельного потока сетевого стека: приём и DHCP-ретраи крутятся
     * в главном цикле вместе с опросом RTL8139.
     */
    while (1) {
        rtl8139_poll();
        rtl8139_handle_receive();
        net_periodic();
        asm volatile("pause");
    }
}
