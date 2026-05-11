#include "apps/editor/editor_render.h"
#include "apps/editor/editor_cursor.h"
#include "kernel/terminal/terminal.h"

void editor_render(const editor_state_t* state) {
    int cursor_row;
    int cursor_col;

    terminal_begin_batch();
    terminal_clear();
    terminal_write("Editing: ");
    terminal_write(state->name);
    terminal_write(".");
    terminal_write(state->ext);
    terminal_write("\n");
    terminal_write("F2 = save, ESC = exit\n");
    if (state->status_message && state->status_message[0]) {
        terminal_write(state->status_message);
    }
    terminal_write("\n");

    for (int i = 0; i < state->len; i++) {
        terminal_put_char(state->buffer[i]);
    }

    editor_cursor_get_visual_position(state, &cursor_row, &cursor_col);
    terminal_set_cursor_absolute(cursor_row, cursor_col);
    terminal_end_batch();
}
