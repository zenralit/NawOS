#include "screen.h"
#include "ports.h"
#include <stdint.h>
#define VGA_ADDRESS 0xB8000
#define MAX_ROWS 25
#define MAX_COLS 80
#define WHITE_ON_BLACK 0x0F
//uint16_t* VIDEO_MEMORY = (uint16_t*) VGA_ADDRESS;
static uint16_t* const VIDEO_MEMORY = (uint16_t*)0xB8000;
uint16_t cursor_offset = 0;



void clear_screen() {
     for (int i = 0; i < 80 * 25; i++) {
        VIDEO_MEMORY[i] = (WHITE_ON_BLACK << 8) | ' ';
    }
    cursor_offset = 0;
}

void put_char(char c) {
    if (c == '\n') {
        cursor_offset += (MAX_COLS * 2) - (cursor_offset % (MAX_COLS * 2));
    } else if (c == '\b') {
        if (cursor_offset >= 2) {
            cursor_offset -= 2;
            VIDEO_MEMORY[cursor_offset / 2] = (WHITE_ON_BLACK << 8) | ' ';
        }
    } else {
        VIDEO_MEMORY[cursor_offset / 2] = (WHITE_ON_BLACK << 8) | c;
        cursor_offset += 2;
    }

    if (cursor_offset >= MAX_ROWS * MAX_COLS * 2) {
        clear_screen();  
    }

    update_cursor();
}

void print(const char* str) {
    while (*str) {
        put_char(*str++);
    }
}

void print_backspace() {
    put_char('\b');
}

#define MAX_COLS 80
#define MAX_ROWS 25
#define VIDEO_ADDRESS 0xB8000

extern uint16_t cursor_offset;

void update_cursor() {
    uint16_t pos = cursor_offset / 2;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

uint8_t get_scancode() {
    uint8_t scancode;
    do {
        asm volatile ("inb %1, %0" : "=a"(scancode) : "Nd"(0x60));
    } while (scancode >= 128);  
    return scancode;
}
