#include "apps/editor/editor_cursor.h"
#include "kernel/input/keyboard_driver.h"
#include "kernel/terminal/terminal.h"

static void editor_step_visual_position(char c, int* row, int* col) {
    if (c == '\n') {
        (*row)++;
        *col = 0;
        return;
    }

    (*col)++;
    if (*col >= TERMINAL_COLS) {
        (*row)++;
        *col = 0;
    }
}

void editor_cursor_get_visual_position(const editor_state_t* state, int* row, int* col) {
    *row = EDITOR_TEXT_START_ROW;
    *col = 0;

    for (int i = 0; i < state->cursor; i++) {
        editor_step_visual_position(state->buffer[i], row, col);
    }
}

int editor_cursor_index_from_visual_position(const editor_state_t* state, int target_row, int target_col) {
    int row = EDITOR_TEXT_START_ROW;
    int col = 0;
    int best_index = -1;

    for (int i = 0; i <= state->len; i++) {
        if (row == target_row) {
            best_index = i;
            if (col >= target_col) {
                return i;
            }
        } else if (row > target_row) {
            break;
        }

        if (i == state->len) {
            break;
        }

        editor_step_visual_position(state->buffer[i], &row, &col);
    }

    return best_index;
}

void editor_cursor_move(editor_state_t* state, uint8_t extended_scancode) {
    if (extended_scancode == KEYBOARD_EXTENDED_KEY_LEFT && state->cursor > 0) {
        state->cursor--;
    } else if (extended_scancode == KEYBOARD_EXTENDED_KEY_RIGHT && state->cursor < state->len) {
        state->cursor++;
    } else if (extended_scancode == KEYBOARD_EXTENDED_KEY_UP) {
        int row;
        int col;
        int next_cursor;

        editor_cursor_get_visual_position(state, &row, &col);
        next_cursor = editor_cursor_index_from_visual_position(state, row - 1, col);
        if (next_cursor >= 0) {
            state->cursor = next_cursor;
        }
    } else if (extended_scancode == KEYBOARD_EXTENDED_KEY_DOWN) {
        int row;
        int col;
        int next_cursor;

        editor_cursor_get_visual_position(state, &row, &col);
        next_cursor = editor_cursor_index_from_visual_position(state, row + 1, col);
        if (next_cursor >= 0) {
            state->cursor = next_cursor;
        }
    }
}
