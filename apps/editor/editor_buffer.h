#ifndef APPS_EDITOR_EDITOR_BUFFER_H
#define APPS_EDITOR_EDITOR_BUFFER_H

#include "apps/editor/editor.h"

int editor_buffer_insert(editor_state_t* state, char c);
int editor_buffer_delete(editor_state_t* state);

#endif
