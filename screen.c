#include "screen.h"
#include "ports.h"
#include <stdint.h>
#include "ip.h"

#define VGA_ADDRESS 0xB8000
#define MAX_ROWS 25
#define MAX_COLS 80
#define WHITE_ON_BLACK 0x0F
#define VIDEO_ADDRESS 0xB8000

static uint16_t* const VIDEO_MEMORY = (uint16_t*)0xB8000;
extern uint16_t cursor_offset;
uint16_t cursor_offset = 0;
int cursor_x = 0;
int cursor_y = 0;

int16_t get_cursor_offset() {
    port_byte_out(0x3D4, 14);
    uint16_t offset = port_byte_in(0x3D5) << 8;
    port_byte_out(0x3D4, 15);
    offset += port_byte_in(0x3D5);
    return offset;
}

void set_cursor_offset(uint16_t offset) {
    port_byte_out(0x3D4, 14);
    port_byte_out(0x3D5, (offset >> 8) & 0xFF);
    port_byte_out(0x3D4, 15);
    port_byte_out(0x3D5, offset & 0xFF);
}

void move_cursor_left() {
    uint16_t offset = get_cursor_offset();
    if (offset > 0) set_cursor_offset(offset - 1);
}

void move_cursor_right() {
    uint16_t offset = get_cursor_offset();
    if (offset < 80 * 25 - 1) set_cursor_offset(offset + 1);
}


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

void update_cursor() {
    uint16_t pos = cursor_offset / 2;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

uint8_t get_scancode() {
    // uint8_t scancode;
    // do {
    //     asm volatile ("inb %1, %0" : "=a"(scancode) : "Nd"(0x60));
    // } while (scancode >= 128);  
    // return scancode;

    // uint8_t scancode;
    // asm volatile ("inb %1, %0" : "=a"(scancode) : "Nd"(0x60));
    // return scancode;

    uint8_t sc = inb(0x60);
    if (sc == 0xE0) {
        uint8_t next = inb(0x60);
        return (0xE0 << 8) | next;
    }
    return sc;
}

void print_double(double value) {
    int int_part = (int)value;
    int frac_part = (int)((value - int_part) * 10000); 
    print_int(int_part);
    print(".");
    if (frac_part < 0) frac_part = -frac_part;
    if (frac_part < 1000) print("0");  
    if (frac_part < 100) print("0");
    if (frac_part < 10) print("0");
    print_int(frac_part);
}
void print_int(int num) {
    char buf[12]; 
    int i = 0;

    if (num == 0) {
        put_char('0');
        return;
    }

    if (num < 0) {
        put_char('-');
        num = -num;
    }

    while (num > 0) {
        buf[i++] = '0' + (num % 10);
        num /= 10;
    }

    while (i--) {
        put_char(buf[i]);
    }
}
void print_hex(uint16_t value) {
    char hex_digits[] = "0123456789ABCDEF";
    char output[7]; 
    output[0] = '0';
    output[1] = 'x';
    output[2] = hex_digits[(value >> 12) & 0xF];
    output[3] = hex_digits[(value >> 8) & 0xF];
    output[4] = hex_digits[(value >> 4) & 0xF];
    output[5] = hex_digits[value & 0xF];
    output[6] = '\0';
    print(output);
}
void print_dec(uint32_t num) {
    char buffer[12];
    int i = 0;

    if (num == 0) {
        print("0");
        return;
    }

    while (num > 0) {
        buffer[i++] = '0' + (num % 10);
        num /= 10;
    }

    for (i--; i >= 0; i--) {
        put_char(buffer[i]);
    }
}

void print_ip() {
    print_hex(naw_ip_address[0]);
    print(".");
    print_hex(naw_ip_address[1]);
    print(".");
    print_hex(naw_ip_address[2]);
    print(".");
    print_hex(naw_ip_address[3]);
}