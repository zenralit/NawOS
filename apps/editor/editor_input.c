#include "apps/editor/editor_input.h"
#include "apps/editor/editor_buffer.h"
#include "apps/editor/editor_cursor.h"
#include "apps/editor/editor_file.h"

editor_input_result_t editor_input_handle_event(editor_state_t* state, const keyboard_event_t* event) {
    if (!event || !event->pressed) {
        return EDITOR_INPUT_IGNORED;
    }

    if (event->extended) {
        editor_cursor_move(state, (uint8_t)(event->scancode & 0xFF));
        return EDITOR_INPUT_RENDER;
    }

    if (event->scancode == 60) {
        if (editor_file_save(state) == 0) {
            state->status_message = "File saved";
        } else {
            state->status_message = "Save failed";
        }
        return EDITOR_INPUT_RENDER;
    }

    if (event->scancode == 1) {
        return EDITOR_INPUT_EXIT;
    }

    if (event->character == '\n') {
        if (editor_buffer_insert(state, '\n')) {
            state->status_message = "";
        } else {
            state->status_message = "Buffer full";
        }
        return EDITOR_INPUT_RENDER;
    }

    if (event->character == '\b') {
        editor_buffer_delete(state);
        state->status_message = "";
        return EDITOR_INPUT_RENDER;
    }

    if (event->character == '\t') {
        int inserted = 0;

        while (inserted < 4 && editor_buffer_insert(state, ' ')) {
            inserted++;
        }

        state->status_message = (inserted == 4) ? "" : "Buffer full";
        return EDITOR_INPUT_RENDER;
    }

    if (event->character >= 32 && event->character <= 126) {
        if (editor_buffer_insert(state, event->character)) {
            state->status_message = "";
        } else {
            state->status_message = "Buffer full";
        }
        return EDITOR_INPUT_RENDER;
    }

    return EDITOR_INPUT_IGNORED;
}
