#include "kernel/shell/shell.h"
#include "kernel/shell/shell_commands.h"
#include "kernel/terminal/terminal.h"

void shell_print_prompt() {
    terminal_write("\n> ");
}

void shell_run_command(const char* input) {
    shell_execute_command(input);
    shell_print_prompt();
}
