#ifndef SCREEN_H
#define SCREEN_H

#include <stdint.h>

extern uint16_t cursor_offset;
void update_cursor();
void clear_screen();
void put_char(char c);
void print(const char* s);
void print_backspace();
void print_double(double value);
void print_int(int num);

#endif

