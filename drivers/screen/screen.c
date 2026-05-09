#include "screen.h"
#include "drivers/ports/ports.h"
#include <stdint.h>
#include "drivers/net/ip.h"

#define VGA_ADDRESS 0xB8000
#define MAX_ROWS SCREEN_ROWS
#define MAX_COLS SCREEN_COLS
#define SCROLLBACK_ROWS 96
#define WHITE_ON_BLACK 0x0F
#define VIDEO_ADDRESS 0xB8000

static uint16_t* const VIDEO_MEMORY = (uint16_t*)0xB8000;
extern uint16_t cursor_offset;
uint16_t cursor_offset = 0;
int cursor_x = 0;
int cursor_y = 0;
static uint16_t screen_history[SCROLLBACK_ROWS][MAX_COLS];
static uint8_t line_lengths[SCROLLBACK_ROWS];
static uint8_t line_wrapped[SCROLLBACK_ROWS];
static int cursor_row = 0;
static int cursor_col = 0;
static int total_rows = 1;
static int view_top_row = 0;
static int render_batch_depth = 0;
static int render_pending = 0;

int16_t get_cursor_offset() {
    return cursor_offset / 2;
}

static uint16_t make_cell(char c) {
    return (WHITE_ON_BLACK << 8) | (uint8_t)c;
}

static void clear_history_row(int row) {
    for (int col = 0; col < MAX_COLS; col++) {
        screen_history[row][col] = make_cell(' ');
    }
    line_lengths[row] = 0;
    line_wrapped[row] = 0;
}

static void reset_history_buffer() {
    for (int row = 0; row < SCROLLBACK_ROWS; row++) {
        clear_history_row(row);
    }
}

static void shift_history_up() {
    for (int row = 1; row < SCROLLBACK_ROWS; row++) {
        for (int col = 0; col < MAX_COLS; col++) {
            screen_history[row - 1][col] = screen_history[row][col];
        }
        line_lengths[row - 1] = line_lengths[row];
        line_wrapped[row - 1] = line_wrapped[row];
    }

    clear_history_row(SCROLLBACK_ROWS - 1);

    if (cursor_row > 0) {
        cursor_row--;
    }
    if (total_rows > 1) {
        total_rows--;
    }
    if (view_top_row > 0) {
        view_top_row--;
    }
}

static int live_view_top_row() {
    if (total_rows <= MAX_ROWS) {
        return 0;
    }
    return total_rows - MAX_ROWS;
}

static void sync_view_to_bottom() {
    view_top_row = live_view_top_row();
}

static void ensure_cursor_visible() {
    if (cursor_row < view_top_row) {
        view_top_row = cursor_row;
    } else if (cursor_row >= view_top_row + MAX_ROWS) {
        view_top_row = cursor_row - MAX_ROWS + 1;
    }
}

static void render_view() {
    for (int row = 0; row < MAX_ROWS; row++) {
        int history_row = view_top_row + row;

        for (int col = 0; col < MAX_COLS; col++) {
            if (history_row < total_rows) {
                VIDEO_MEMORY[row * MAX_COLS + col] = screen_history[history_row][col];
            } else {
                VIDEO_MEMORY[row * MAX_COLS + col] = make_cell(' ');
            }
        }
    }

    if (cursor_row >= view_top_row && cursor_row < view_top_row + MAX_ROWS) {
        cursor_offset = ((cursor_row - view_top_row) * MAX_COLS + cursor_col) * 2;
    } else {
        cursor_offset = ((MAX_ROWS - 1) * MAX_COLS + (MAX_COLS - 1)) * 2;
    }

    cursor_x = cursor_col;
    cursor_y = cursor_row;
    update_cursor();
}

static void request_render() {
    if (render_batch_depth > 0) {
        render_pending = 1;
        return;
    }

    render_view();
}

static void ensure_history_room() {
    if (cursor_row >= SCROLLBACK_ROWS) {
        shift_history_up();
        cursor_row = SCROLLBACK_ROWS - 1;
    }

    if (cursor_row >= total_rows) {
        total_rows = cursor_row + 1;
    }
}

static void advance_to_next_row() {
    cursor_col = 0;
    cursor_row++;
    ensure_history_room();
    clear_history_row(cursor_row);
}

void set_cursor_offset(uint16_t offset) {
    if (offset >= MAX_ROWS * MAX_COLS) {
        offset = (MAX_ROWS * MAX_COLS) - 1;
    }

    cursor_row = view_top_row + (offset / MAX_COLS);
    cursor_col = offset % MAX_COLS;
    ensure_history_room();
    request_render();
}

void move_cursor_left() {
    if (cursor_col > 0) {
        cursor_col--;
    } else if (cursor_row > 0) {
        int prev_row = cursor_row - 1;
        cursor_row = prev_row;
        cursor_col = line_wrapped[prev_row] ? (MAX_COLS - 1) : line_lengths[prev_row];
    } else {
        return;
    }

    ensure_cursor_visible();
    request_render();
}

void move_cursor_right() {
    if (line_wrapped[cursor_row]) {
        if (cursor_col < MAX_COLS - 1) {
            cursor_col++;
        } else if (cursor_row + 1 < total_rows) {
            cursor_row++;
            cursor_col = 0;
        } else {
            return;
        }
    } else if (cursor_col < line_lengths[cursor_row]) {
        cursor_col++;
    } else if (cursor_row + 1 < total_rows) {
        cursor_row++;
        cursor_col = 0;
    } else {
        return;
    }

    ensure_cursor_visible();
    request_render();
}

void screen_set_cursor_absolute(int row, int col) {
    if (row < 0) {
        row = 0;
    }
    if (row >= total_rows) {
        row = total_rows - 1;
    }
    if (row < 0) {
        row = 0;
    }

    if (col < 0) {
        col = 0;
    }
    if (col >= MAX_COLS) {
        col = MAX_COLS - 1;
    }

    cursor_row = row;
    cursor_col = col;
    ensure_cursor_visible();
    request_render();
}

void screen_begin_batch() {
    render_batch_depth++;
}

void screen_end_batch() {
    if (render_batch_depth == 0) {
        return;
    }

    render_batch_depth--;
    if (render_batch_depth == 0 && render_pending) {
        render_pending = 0;
        render_view();
    }
}

void clear_screen() {
    reset_history_buffer();
    cursor_row = 0;
    cursor_col = 0;
    total_rows = 1;
    view_top_row = 0;
    cursor_offset = 0;
    request_render();
}

void screen_scroll_page_up() {
    if (view_top_row == 0) {
        return;
    }

    view_top_row -= MAX_ROWS;
    if (view_top_row < 0) {
        view_top_row = 0;
    }

    request_render();
}

void screen_scroll_page_down() {
    int bottom_view = live_view_top_row();

    if (view_top_row >= bottom_view) {
        return;
    }

    view_top_row += MAX_ROWS;
    if (view_top_row > bottom_view) {
        view_top_row = bottom_view;
    }

    request_render();
}

void put_char(char c) {
    if (c == '\n') {
        line_lengths[cursor_row] = cursor_col;
        line_wrapped[cursor_row] = 0;
        advance_to_next_row();
    } else if (c == '\b') {
        if (cursor_row > 0 || cursor_col > 0) {
            if (cursor_col > 0) {
                cursor_col--;
            } else {
                int prev_row = cursor_row - 1;
                cursor_row = prev_row;
                cursor_col = line_wrapped[prev_row] ? (MAX_COLS - 1) : line_lengths[prev_row];
            }

            if (cursor_col < 0) {
                cursor_col = 0;
            }

            if (line_wrapped[cursor_row] && cursor_col == MAX_COLS - 1) {
                line_wrapped[cursor_row] = 0;
                line_lengths[cursor_row] = MAX_COLS - 1;
            } else {
                line_lengths[cursor_row] = cursor_col;
            }

            screen_history[cursor_row][cursor_col] = make_cell(' ');
        }
    } else {
        screen_history[cursor_row][cursor_col] = make_cell(c);
        cursor_col++;
        line_lengths[cursor_row] = cursor_col;

        if (cursor_col >= MAX_COLS) {
            line_wrapped[cursor_row] = 1;
            advance_to_next_row();
        } else {
            line_wrapped[cursor_row] = 0;
        }
    }

    sync_view_to_bottom();
    request_render();
}

void print(const char* str) {
    while (*str) {
        put_char(*str++);
    }
}

void print_backspace() {
    put_char('\b');
}

void update_cursor() {
    uint16_t pos = cursor_offset / 2;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

uint16_t get_scancode() {
    if ((inb(0x64) & 0x01) == 0) {
        return 0;
    }

    uint8_t sc = inb(0x60);
    if (sc == 0xE0) {
        while ((inb(0x64) & 0x01) == 0) {
        }
        uint8_t next = inb(0x60);
        return 0xE000 | next;
    }
    return sc;
}

void print_double(double value) {
    int int_part = (int)value;
    int frac_part = (int)((value - int_part) * 10000); 
    print_int(int_part);
    print(".");
    if (frac_part < 0) frac_part = -frac_part;
    if (frac_part < 1000) print("0");  
    if (frac_part < 100) print("0");
    if (frac_part < 10) print("0");
    print_int(frac_part);
}
void print_int(int num) {
    char buf[12]; 
    int i = 0;

    if (num == 0) {
        put_char('0');
        return;
    }

    if (num < 0) {
        put_char('-');
        num = -num;
    }

    while (num > 0) {
        buf[i++] = '0' + (num % 10);
        num /= 10;
    }

    while (i--) {
        put_char(buf[i]);
    }
}
void print_tab(){
    
    put_char(' ');

}

void print_hex(uint16_t value) {
    char hex_digits[] = "0123456789ABCDEF";
    char output[7]; 
    output[0] = '0';
    output[1] = 'x';
    output[2] = hex_digits[(value >> 12) & 0xF];
    output[3] = hex_digits[(value >> 8) & 0xF];
    output[4] = hex_digits[(value >> 4) & 0xF];
    output[5] = hex_digits[value & 0xF];
    output[6] = '\0';
    print(output);
}
void print_dec(uint32_t num) {
    char buffer[12];
    int i = 0;

    if (num == 0) {
        print("0");
        return;
    }

    while (num > 0) {
        buffer[i++] = '0' + (num % 10);
        num /= 10;
    }

    for (i--; i >= 0; i--) {
        put_char(buffer[i]);
    }
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
