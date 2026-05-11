#include "apps/editor/editor_file.h"
#include "fs/nawfs.h"

void editor_file_load(editor_state_t* state) {
    int loaded = fs_read_to_buffer(state->name, state->ext, state->buffer, sizeof(state->buffer));

    if (loaded >= 0) {
        state->len = loaded;
        state->cursor = loaded;
        state->status_message = "";
    } else {
        state->status_message = "New file";
    }
}

int editor_file_save(const editor_state_t* state) {
    return fs_write(state->name, state->ext, state->buffer);
}
