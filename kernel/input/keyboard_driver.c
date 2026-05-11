#include "kernel/input/keyboard_driver.h"
#include "drivers/ports/ports.h"

#define PORT_KBD_DATA 0x60
#define EXTENDED_SCANCODE_PREFIX 0xE0

static const char scancode_map[128] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n', 0,
    'a','s','d','f','g','h','j','k','l',';','\'','`', 0, '\\',
    'z','x','c','v','b','n','m',',','.','/', 0, '*',
    0, ' ', 0,
};

static const char scancode_shift_map[128] = {
    0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n', 0,
    'A','S','D','F','G','H','J','K','L',':','"','~', 0, '|',
    'Z','X','C','V','B','N','M','<','>','?', 0, '*',
    0, ' ', 0,
};

static uint8_t key_down[128];
static uint8_t extended_key_down[128];
static uint8_t extended_scancode_prefix = 0;
static int shift_pressed = 0;
static keyboard_event_handler_t keyboard_event_handler = 0;

void keyboard_driver_init() {
    uint8_t mask = inb(0x21);

    outb(0x21, mask & ~0x02);
}

void keyboard_driver_reset_state() {
    for (int i = 0; i < 128; i++) {
        key_down[i] = 0;
        extended_key_down[i] = 0;
    }

    shift_pressed = 0;
    extended_scancode_prefix = 0;
}

void keyboard_driver_set_event_handler(keyboard_event_handler_t handler) {
    keyboard_event_handler = handler;
}

uint16_t keyboard_driver_read_scancode() {
    if ((inb(0x64) & 0x01) == 0) {
        return 0;
    }

    {
        uint8_t scancode = inb(PORT_KBD_DATA);

        if (scancode == EXTENDED_SCANCODE_PREFIX) {
            while ((inb(0x64) & 0x01) == 0) {
            }

            return 0xE000 | inb(PORT_KBD_DATA);
        }

        return scancode;
    }
}

int keyboard_driver_translate_scancode(uint16_t scancode, keyboard_event_t* event) {
    if (!event || scancode == 0) {
        return 0;
    }

    event->scancode = scancode;
    event->character = 0;
    event->pressed = 0;
    event->extended = 0;

    if (scancode > 0xFF) {
        uint8_t ext = scancode & 0xFF;
        uint8_t key = ext & 0x7F;

        event->extended = 1;

        if (ext & 0x80) {
            extended_key_down[key] = 0;
            return 0;
        }

        if (extended_key_down[key]) {
            return 0;
        }

        extended_key_down[key] = 1;
        event->pressed = 1;
        return 1;
    }

    if (scancode == 42 || scancode == 54) {
        shift_pressed = 1;
        key_down[scancode] = 1;
        event->pressed = 1;
        return 1;
    }

    if (scancode == (42 | 0x80) || scancode == (54 | 0x80)) {
        shift_pressed = 0;
        key_down[scancode & 0x7F] = 0;
        return 0;
    }

    if (scancode & 0x80) {
        key_down[scancode & 0x7F] = 0;
        return 0;
    }

    if (key_down[scancode]) {
        return 0;
    }

    key_down[scancode] = 1;
    event->pressed = 1;
    event->character = shift_pressed ? scancode_shift_map[scancode] : scancode_map[scancode];
    return 1;
}

int keyboard_driver_poll_event(keyboard_event_t* event) {
    return keyboard_driver_translate_scancode(keyboard_driver_read_scancode(), event);
}

void keyboard_driver_handle_irq() {
    uint8_t scancode = port_byte_in(PORT_KBD_DATA);
    uint16_t full_scancode;
    keyboard_event_t event;

    if (scancode == EXTENDED_SCANCODE_PREFIX) {
        extended_scancode_prefix = 1;
        return;
    }

    if (extended_scancode_prefix) {
        full_scancode = 0xE000 | scancode;
        extended_scancode_prefix = 0;
    } else {
        full_scancode = scancode;
    }

    if (keyboard_driver_translate_scancode(full_scancode, &event) && keyboard_event_handler) {
        keyboard_event_handler(&event);
    }
}
