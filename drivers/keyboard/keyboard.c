#include "keyboard.h"
#include "drivers/ports/ports.h"
#include "kernel/input/input.h"
#include "kernel/input/keyboard_driver.h"

void keyboard_handle_scancode(uint16_t scancode) {
    keyboard_event_t event;

    if (keyboard_driver_translate_scancode(scancode, &event)) {
        input_handle_keyboard_event(&event);
    }
}

void keyboard_init() {
    keyboard_driver_init();
    input_init();
}

uint16_t get_scancode() {
    return keyboard_driver_read_scancode();
}

int keyboard_read_int() {
    return input_read_int();
}

void keyboard_handle_interrupt() {
    keyboard_driver_handle_irq();
    /* EOI master PIC после обработки IRQ1. */
    port_byte_out(0x20, 0x20);
}
