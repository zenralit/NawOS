#ifndef APPS_EDITOR_EDITOR_INPUT_H
#define APPS_EDITOR_EDITOR_INPUT_H

#include "apps/editor/editor.h"
#include "kernel/input/keyboard_driver.h"

typedef enum {
    EDITOR_INPUT_IGNORED = 0,
    EDITOR_INPUT_RENDER = 1,
    EDITOR_INPUT_EXIT = 2,
} editor_input_result_t;

editor_input_result_t editor_input_handle_event(editor_state_t* state, const keyboard_event_t* event);

#endif
