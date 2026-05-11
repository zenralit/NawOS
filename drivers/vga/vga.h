#ifndef DRIVERS_VGA_VGA_H
#define DRIVERS_VGA_VGA_H

#include <stdint.h>

#define VGA_ROWS 25
#define VGA_COLS 80
#define VGA_DEFAULT_ATTRIBUTE 0x0F

uint16_t vga_make_cell(char c, uint8_t attribute);
void vga_write_cell(int row, int col, uint16_t cell);
void vga_set_hardware_cursor(int row, int col);

#endif
