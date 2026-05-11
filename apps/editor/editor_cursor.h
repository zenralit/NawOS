#ifndef APPS_EDITOR_EDITOR_CURSOR_H
#define APPS_EDITOR_EDITOR_CURSOR_H

#include <stdint.h>
#include "apps/editor/editor.h"

void editor_cursor_get_visual_position(const editor_state_t* state, int* row, int* col);
int editor_cursor_index_from_visual_position(const editor_state_t* state, int target_row, int target_col);
void editor_cursor_move(editor_state_t* state, uint8_t extended_scancode);

#endif
