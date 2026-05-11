#ifndef KERNEL_INPUT_INPUT_H
#define KERNEL_INPUT_INPUT_H

#include "kernel/input/keyboard_driver.h"

void input_init();
void input_set_handler(keyboard_event_handler_t handler);
void input_handle_keyboard_event(const keyboard_event_t* event);
int input_read_int();

#endif
