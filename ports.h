#ifndef PORTS_H
#define PORTS_H

#include <stdint.h>


static inline uint8_t inb(uint16_t port) {
    uint8_t result;
    __asm__ volatile ("inb %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline void io_wait() {
    __asm__ volatile ("outb %%al, $0x80" : : "a"(0));
}

uint16_t inw(uint16_t port);
void outw(uint16_t port, uint16_t val);

uint8_t port_byte_in(uint16_t port);
void port_byte_out(uint16_t port, uint8_t data);
uint16_t port_word_in(uint16_t port);
void port_word_out(uint16_t port, uint16_t data);

uint32_t inl(uint16_t port);
void outl(uint16_t port, uint32_t val);

#define ATA_PRIMARY_IO 0x1F0
#define ATA_MASTER     0xE0

#endif
