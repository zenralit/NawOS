#include "kernel/shell/shell_input.h"
#include "kernel/shell/shell.h"
#include "kernel/terminal/terminal.h"

#define SHELL_INPUT_BUFFER_SIZE 256

static char shell_input_buffer[SHELL_INPUT_BUFFER_SIZE];
static int shell_input_pos = 0;

void shell_input_init() {
    shell_input_pos = 0;
    shell_input_buffer[0] = 0;
}

void shell_input_handle_event(const keyboard_event_t* event) {
    if (!event || !event->pressed || event->extended || !event->character) {
        return;
    }

    if (event->character == '\b') {
        if (shell_input_pos > 0) {
            shell_input_pos--;
            shell_input_buffer[shell_input_pos] = 0;
            terminal_backspace();
        }
        return;
    }

    if (event->character == '\n') {
        terminal_write("\n");
        shell_input_buffer[shell_input_pos] = 0;
        shell_run_command(shell_input_buffer);
        shell_input_pos = 0;
        shell_input_buffer[0] = 0;
        return;
    }

    if (shell_input_pos < SHELL_INPUT_BUFFER_SIZE - 1) {
        char out[2] = {event->character, 0};

        shell_input_buffer[shell_input_pos++] = event->character;
        shell_input_buffer[shell_input_pos] = 0;
        terminal_write(out);
    }
}
