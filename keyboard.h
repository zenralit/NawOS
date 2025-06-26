#include <stddef.h>
#include <stdint.h>

#ifndef KEYBOARD_H
#define KEYBOARD_H

extern void update_cursor();

uint8_t get_scancode(); 
void init_keyboard();
void start_text_editor(); 
void keyboard_input();
void keyboard_handler();
void keyboard_init();
void keyboard_handle_scancode(uint8_t scancode);
int strcmp(const char* s1, const char* s2);
int strncmp(const char* s1, const char* s2, size_t n);
void keyboard_handle_interrupt();
#endif