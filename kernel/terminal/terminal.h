#ifndef KERNEL_TERMINAL_TERMINAL_H
#define KERNEL_TERMINAL_TERMINAL_H

#include <stdint.h>
#include "drivers/vga/vga.h"

#define TERMINAL_ROWS VGA_ROWS
#define TERMINAL_COLS VGA_COLS

extern int terminal_cursor_x;
extern int terminal_cursor_y;
extern uint16_t terminal_cursor_offset;

void terminal_move_cursor_left();
void terminal_move_cursor_right();
void terminal_set_cursor_absolute(int row, int col);
void terminal_begin_batch();
void terminal_end_batch();
void terminal_scroll_page_up();
void terminal_scroll_page_down();
void terminal_update_cursor();
void terminal_clear();
void terminal_put_char(char c);
void terminal_write(const char* s);
void terminal_backspace();
void terminal_write_double(double value);
void terminal_write_int(int num);
void terminal_write_hex(uint16_t value);
int16_t terminal_get_cursor_offset();
void terminal_set_cursor_offset(uint16_t offset);
void terminal_write_dec(uint32_t num);
void terminal_write_tab();

#endif
