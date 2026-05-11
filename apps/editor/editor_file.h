#ifndef APPS_EDITOR_EDITOR_FILE_H
#define APPS_EDITOR_EDITOR_FILE_H

#include "apps/editor/editor.h"

void editor_file_load(editor_state_t* state);
int editor_file_save(const editor_state_t* state);

#endif
