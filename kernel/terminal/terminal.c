#include "kernel/terminal/terminal.h"
#include "drivers/vga/vga.h"

static const uint8_t terminal_attribute = VGA_DEFAULT_ATTRIBUTE;

#define MAX_ROWS TERMINAL_ROWS
#define MAX_COLS TERMINAL_COLS
/* История строк шире экрана: прокрутка Page Up/Down без потери вывода shell. */
#define SCROLLBACK_ROWS 96

int terminal_cursor_x = 0;
int terminal_cursor_y = 0;
uint16_t terminal_cursor_offset = 0;

static uint16_t screen_history[SCROLLBACK_ROWS][MAX_COLS];
static uint8_t line_lengths[SCROLLBACK_ROWS];
static uint8_t line_wrapped[SCROLLBACK_ROWS];
static int cursor_row = 0;
static int cursor_col = 0;
static int total_rows = 1;
static int view_top_row = 0;
static int render_batch_depth = 0;
static int render_pending = 0;

int16_t terminal_get_cursor_offset() {
    return terminal_cursor_offset / 2;
}

static uint16_t terminal_make_cell(char c) {
    return vga_make_cell(c, terminal_attribute);
}

static void clear_history_row(int row) {
    for (int col = 0; col < MAX_COLS; col++) {
        screen_history[row][col] = terminal_make_cell(' ');
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
                vga_write_cell(row, col, screen_history[history_row][col]);
            } else {
                vga_write_cell(row, col, terminal_make_cell(' '));
            }
        }
    }

    if (cursor_row >= view_top_row && cursor_row < view_top_row + MAX_ROWS) {
        terminal_cursor_offset = ((cursor_row - view_top_row) * MAX_COLS + cursor_col) * 2;
    } else {
        terminal_cursor_offset = ((MAX_ROWS - 1) * MAX_COLS + (MAX_COLS - 1)) * 2;
    }

    terminal_cursor_x = cursor_col;
    terminal_cursor_y = cursor_row;
    terminal_update_cursor();
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

void terminal_set_cursor_offset(uint16_t offset) {
    if (offset >= MAX_ROWS * MAX_COLS) {
        offset = (MAX_ROWS * MAX_COLS) - 1;
    }

    cursor_row = view_top_row + (offset / MAX_COLS);
    cursor_col = offset % MAX_COLS;
    ensure_history_room();
    request_render();
}

void terminal_move_cursor_left() {
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

void terminal_move_cursor_right() {
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

void terminal_set_cursor_absolute(int row, int col) {
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

void terminal_begin_batch() {
    render_batch_depth++;
}

void terminal_end_batch() {
    if (render_batch_depth == 0) {
        return;
    }

    render_batch_depth--;
    /* Отложенный render_view: редактор пишет много символов за один кадр. */
    if (render_batch_depth == 0 && render_pending) {
        render_pending = 0;
        render_view();
    }
}

void terminal_clear() {
    reset_history_buffer();
    cursor_row = 0;
    cursor_col = 0;
    total_rows = 1;
    view_top_row = 0;
    terminal_cursor_offset = 0;
    request_render();
}

void terminal_scroll_page_up() {
    if (view_top_row == 0) {
        return;
    }

    view_top_row -= MAX_ROWS;
    if (view_top_row < 0) {
        view_top_row = 0;
    }

    request_render();
}

void terminal_scroll_page_down() {
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

void terminal_put_char(char c) {
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

            screen_history[cursor_row][cursor_col] = terminal_make_cell(' ');
        }
    } else {
        screen_history[cursor_row][cursor_col] = terminal_make_cell(c);
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

void terminal_write(const char* str) {
    while (*str) {
        terminal_put_char(*str++);
    }
}

void terminal_backspace() {
    terminal_put_char('\b');
}

void terminal_update_cursor() {
    vga_set_hardware_cursor(terminal_cursor_offset / 2 / MAX_COLS, terminal_cursor_offset / 2 % MAX_COLS);
}

void terminal_write_double(double value) {
    int negative = 0;
    int int_part;
    int frac_part;

    if (value < 0.0) {
        negative = 1;
        value = -value;
    }

    /* 4 знака после точки; округление вверх может дать frac_part == 10000. */
    int_part = (int)value;
    value -= (double)int_part;
    frac_part = (int)(value * 10000.0 + 0.5);

    if (frac_part >= 10000) {
        int_part++;
        frac_part -= 10000;
    }

    if (negative) {
        terminal_put_char('-');
    }

    terminal_write_int(int_part);
    terminal_write(".");

    if (frac_part < 1000) {
        terminal_write("0");
    }

    if (frac_part < 100) {
        terminal_write("0");
    }

    if (frac_part < 10) {
        terminal_write("0");
    }

    terminal_write_int(frac_part);
}

void terminal_write_int(int num) {
    char buf[12];
    int i = 0;

    if (num == 0) {
        terminal_put_char('0');
        return;
    }

    if (num < 0) {
        terminal_put_char('-');
        num = -num;
    }

    while (num > 0) {
        buf[i++] = '0' + (num % 10);
        num /= 10;
    }

    while (i--) {
        terminal_put_char(buf[i]);
    }
}

void terminal_write_tab() {
    terminal_put_char(' ');
}

void terminal_write_hex(uint16_t value) {
    char hex_digits[] = "0123456789ABCDEF";
    char output[7];

    output[0] = '0';
    output[1] = 'x';
    output[2] = hex_digits[(value >> 12) & 0x0F];
    output[3] = hex_digits[(value >> 8) & 0x0F];
    output[4] = hex_digits[(value >> 4) & 0x0F];
    output[5] = hex_digits[value & 0x0F];
    output[6] = '\0';
    terminal_write(output);
}

void terminal_write_dec(uint32_t num) {
    char buffer[12];
    int i = 0;

    if (num == 0) {
        terminal_write("0");
        return;
    }

    while (num > 0) {
        buffer[i++] = '0' + (num % 10);
        num /= 10;
    }

    for (i--; i >= 0; i--) {
        terminal_put_char(buffer[i]);
    }
}
