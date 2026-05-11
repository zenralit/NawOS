#include <stdint.h>

#ifndef KEYBOARD_H
#define KEYBOARD_H

uint16_t get_scancode();
void keyboard_init();
void keyboard_handle_scancode(uint16_t sc);
void keyboard_handle_interrupt();
int keyboard_read_int();

#endif
