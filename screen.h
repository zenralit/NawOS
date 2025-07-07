#ifndef SCREEN_H
#define SCREEN_H

#include <stdint.h>

extern int cursor_x;
extern int cursor_y;
extern uint16_t cursor_offset;

void move_cursor_left();
void move_cursor_right();
void update_cursor();
void clear_screen();
void put_char(char c);
void print(const char* s);
void print_backspace();
void print_double(double value);
void print_int(int num);
void print_hex(uint16_t value);

#endif

