#ifndef KERNEL_INPUT_KEYBOARD_DRIVER_H
#define KERNEL_INPUT_KEYBOARD_DRIVER_H

#include <stdint.h>

#define KEYBOARD_EXTENDED_KEY_UP 0x48
#define KEYBOARD_EXTENDED_KEY_DOWN 0x50
#define KEYBOARD_EXTENDED_KEY_LEFT 0x4B
#define KEYBOARD_EXTENDED_KEY_RIGHT 0x4D

typedef struct {
    uint16_t scancode;
    char character;
    uint8_t pressed;
    uint8_t extended;
} keyboard_event_t;

typedef void (*keyboard_event_handler_t)(const keyboard_event_t* event);

void keyboard_driver_init();
void keyboard_driver_reset_state();
void keyboard_driver_set_event_handler(keyboard_event_handler_t handler);
uint16_t keyboard_driver_read_scancode();
int keyboard_driver_translate_scancode(uint16_t scancode, keyboard_event_t* event);
int keyboard_driver_poll_event(keyboard_event_t* event);
void keyboard_driver_handle_irq();

#endif
