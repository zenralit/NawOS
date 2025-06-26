#include <stdint.h>
#include <stddef.h>
#include "screen.h"
#include "idt.h"
#include "keyboard.h"
#include "ports.h"
#include "nawfs.h"
#include "nawstring.h"
#include "disk.h"

void dummy_timer_callback() {}
    
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