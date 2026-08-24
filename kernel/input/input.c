#include "kernel/input/input.h"
#include "kernel/input/keyboard_driver.h"
#include "kernel/shell/shell_input.h"
#include "kernel/terminal/terminal.h"
#include "lib/nawutil.h"

static keyboard_event_handler_t active_handler = 0;

void input_init() {
    shell_input_init();
    active_handler = shell_input_handle_event;
    keyboard_driver_set_event_handler(input_handle_keyboard_event);
}

void input_set_handler(keyboard_event_handler_t handler) {
    /* Редактор подменяет обработчик; shell восстанавливается через input_init. */
    active_handler = handler;
}

void input_handle_keyboard_event(const keyboard_event_t* event) {
    if (!event || !event->pressed) {
        return;
    }

    /* Page Up/Down на расширенных scancode — прокрутка терминала, не shell. */
    if (event->extended) {
        if ((event->scancode & 0xFF) == KEYBOARD_EXTENDED_KEY_UP) {
            terminal_scroll_page_up();
            return;
        }

        if ((event->scancode & 0xFF) == KEYBOARD_EXTENDED_KEY_DOWN) {
            terminal_scroll_page_down();
            return;
        }
    }

    if (active_handler) {
        active_handler(event);
    }
}

int input_read_int() {
    char buffer[32];
    int pos = 0;
    keyboard_event_t event;

    keyboard_driver_reset_state();
    buffer[0] = 0;

    /* Блокирующий ввод числа для read в Lelya; scancode 28 — Enter. */
    while (1) {
        if (!keyboard_driver_poll_event(&event)) {
            asm volatile("pause");
            continue;
        }

        if (!event.pressed || event.extended) {
            continue;
        }

        if (event.scancode == 28) {
            terminal_write("\n");
            break;
        }

        if (event.character == '\b') {
            if (pos > 0) {
                pos--;
                buffer[pos] = 0;
                terminal_backspace();
            }
            continue;
        }

        if (((event.character >= '0' && event.character <= '9') ||
             event.character == '-' || event.character == '+') &&
            pos < (int)sizeof(buffer) - 1) {
            if ((event.character == '-' || event.character == '+') && pos != 0) {
                continue;
            }

            buffer[pos++] = event.character;
            buffer[pos] = 0;

            {
                char out[2] = {event.character, 0};
                terminal_write(out);
            }
        }
    }

    keyboard_driver_reset_state();
    return naw_atoi(buffer);
}
