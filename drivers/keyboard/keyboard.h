#include <stddef.h>
#include <stdint.h>
#include "drivers/screen/screen.h"

#ifndef KEYBOARD_H
#define KEYBOARD_H

uint8_t get_scancode(); 
void init_keyboard();
void start_text_editor(); 
// void keyboard_input();
void keyboard_handler();
void keyboard_init();
void keyboard_handle_scancode(uint8_t sc);
int strcmp(const char* s1, const char* s2);
int strncmp(const char* s1, const char* s2, size_t n);
void keyboard_handle_interrupt();
int fs_read_to_buffer(const char* name, const char* ext, char* buffer, int max_len);
void start_text_editor(const char* name, const char* ext);
int atoi(const char* str);
char* strstr(const char* haystack, const char* needle);
char* find_char(const char* str, char ch);
void* memcpy(void* dest, const void* src, size_t n);
void uint8_to_hex(uint8_t val, char* out);
void reboot();
void start_calculator();


#endif
