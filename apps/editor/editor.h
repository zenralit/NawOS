#ifndef APPS_EDITOR_EDITOR_H
#define APPS_EDITOR_EDITOR_H

#define EDITOR_BUFFER_SIZE 512
#define EDITOR_TEXT_START_ROW 3

typedef struct {
    const char* name;
    const char* ext;
    char buffer[EDITOR_BUFFER_SIZE];
    int len;
    int cursor;
    const char* status_message;
} editor_state_t;

void editor_start(const char* name, const char* ext);

#endif
