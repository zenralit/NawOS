#include "apps/editor/editor.h"
#include "apps/editor/editor_file.h"
#include "apps/editor/editor_input.h"
#include "apps/editor/editor_render.h"
#include "kernel/input/keyboard_driver.h"
#include "kernel/terminal/terminal.h"

void editor_start(const char* name, const char* ext) {
    editor_state_t state = {0};
    keyboard_event_t event;

    state.name = name;
    state.ext = ext;
    state.status_message = "";

    editor_file_load(&state);

    keyboard_driver_reset_state();
    editor_render(&state);

    while (1) {
        editor_input_result_t result;

        if (!keyboard_driver_poll_event(&event)) {
            continue;
        }

        result = editor_input_handle_event(&state, &event);
        if (result == EDITOR_INPUT_EXIT) {
            break;
        }

        if (result == EDITOR_INPUT_RENDER) {
            editor_render(&state);
        }
    }

    keyboard_driver_reset_state();
    terminal_write("\nExited editor\n");
}
