#ifndef KERNEL_SHELL_SHELL_INPUT_H
#define KERNEL_SHELL_SHELL_INPUT_H

#include "kernel/input/keyboard_driver.h"

void shell_input_init();
void shell_input_handle_event(const keyboard_event_t* event);

#endif
