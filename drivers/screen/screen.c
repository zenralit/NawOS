#include "screen.h"
#include "kernel/terminal/terminal.h"
#include "net/ip.h"

int cursor_x = 0;
int cursor_y = 0;
uint16_t cursor_offset = 0;

static void sync_compatibility_state() {
    cursor_x = terminal_cursor_x;
    cursor_y = terminal_cursor_y;
    cursor_offset = terminal_cursor_offset;
}

void move_cursor_left() {
    terminal_move_cursor_left();
    sync_compatibility_state();
}

void move_cursor_right() {
    terminal_move_cursor_right();
    sync_compatibility_state();
}

void screen_set_cursor_absolute(int row, int col) {
    terminal_set_cursor_absolute(row, col);
    sync_compatibility_state();
}

void screen_begin_batch() {
    terminal_begin_batch();
}

void screen_end_batch() {
    terminal_end_batch();
    sync_compatibility_state();
}

void screen_scroll_page_up() {
    terminal_scroll_page_up();
    sync_compatibility_state();
}

void screen_scroll_page_down() {
    terminal_scroll_page_down();
    sync_compatibility_state();
}

void update_cursor() {
    terminal_update_cursor();
    sync_compatibility_state();
}

void clear_screen() {
    terminal_clear();
    sync_compatibility_state();
}

void put_char(char c) {
    terminal_put_char(c);
    sync_compatibility_state();
}

void print(const char* s) {
    terminal_write(s);
    sync_compatibility_state();
}

void print_backspace() {
    terminal_backspace();
    sync_compatibility_state();
}

void print_double(double value) {
    terminal_write_double(value);
    sync_compatibility_state();
}

void print_int(int num) {
    terminal_write_int(num);
    sync_compatibility_state();
}

void print_hex(uint16_t value) {
    terminal_write_hex(value);
    sync_compatibility_state();
}

int16_t get_cursor_offset() {
    return terminal_get_cursor_offset();
}

void set_cursor_offset(uint16_t offset) {
    terminal_set_cursor_offset(offset);
    sync_compatibility_state();
}

void print_dec(uint32_t num) {
    terminal_write_dec(num);
    sync_compatibility_state();
}

void print_tab() {
    terminal_write_tab();
    sync_compatibility_state();
}

void print_ip() {
    print_hex(naw_ip_address[0]);
    print(".");
    print_hex(naw_ip_address[1]);
    print(".");
    print_hex(naw_ip_address[2]);
    print(".");
    print_hex(naw_ip_address[3]);
}
