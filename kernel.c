#include <stdint.h>
#include <stddef.h>
#include "screen.h"
#include "idt.h"
#include "keyboard.h"
#include "ports.h"
#include "nawfs.h"
#include "nawstring.h"
#include "disk.h"
// extern void sti();
// extern void irq0();
// extern void irq1();

void dummy_timer_callback() {}

//  void irq_handler_0() {}

// void irq_handler_1() {
// }
    
void kernel_main() {
     keyboard_handle_interrupt();
    clear_screen();
    
    print("Welcome in NawOS.\n");
    print("print command >>>>\n");
    idt_init();
    keyboard_init();
     fs_init();

    asm volatile("sti");

    while (1) {  
        asm volatile("hlt");
    }
}