#ifndef SCREEN_H
#define SCREEN_H

#include <stdint.h>
#include "drivers/vga/vga.h"

#define SCREEN_ROWS VGA_ROWS
#define SCREEN_COLS VGA_COLS

extern int cursor_x;
extern int cursor_y;
extern uint16_t cursor_offset;

void move_cursor_left();
void move_cursor_right();
void screen_set_cursor_absolute(int row, int col);
void screen_begin_batch();
void screen_end_batch();
void screen_scroll_page_up();
void screen_scroll_page_down();
void update_cursor();
void clear_screen();
void put_char(char c);
void print(const char* s);
void print_backspace();
void print_double(double value);
void print_int(int num);
void print_hex(uint16_t value);
int16_t get_cursor_offset();
void set_cursor_offset(uint16_t offset);
void print_dec(uint32_t num);
void print_ip();
void print_tab();

#endif
