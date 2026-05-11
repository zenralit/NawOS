#include "drivers/vga/vga.h"
#include "drivers/ports/ports.h"

#define VGA_ADDRESS 0xB8000

static uint16_t* const VGA_MEMORY = (uint16_t*)VGA_ADDRESS;

uint16_t vga_make_cell(char c, uint8_t attribute) {
    return ((uint16_t)attribute << 8) | (uint8_t)c;
}

void vga_write_cell(int row, int col, uint16_t cell) {
    VGA_MEMORY[row * VGA_COLS + col] = cell;
}

void vga_set_hardware_cursor(int row, int col) {
    uint16_t pos = (uint16_t)(row * VGA_COLS + col);

    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}
