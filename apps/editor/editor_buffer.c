#include "apps/editor/editor_buffer.h"

int editor_buffer_insert(editor_state_t* state, char c) {
    if (state->len >= EDITOR_BUFFER_SIZE - 1) {
        return 0;
    }

    for (int i = state->len; i >= state->cursor; i--) {
        state->buffer[i + 1] = state->buffer[i];
    }

    state->buffer[state->cursor] = c;
    state->len++;
    state->cursor++;
    return 1;
}

int editor_buffer_delete(editor_state_t* state) {
    if (state->cursor <= 0) {
        return 0;
    }

    for (int i = state->cursor - 1; i < state->len; i++) {
        state->buffer[i] = state->buffer[i + 1];
    }

    state->cursor--;
    state->len--;
    return 1;
}
