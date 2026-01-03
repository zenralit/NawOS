#pragma once

#ifndef IDT_H
#define IDT_H

#include <stdint.h>

extern void irq0(); 
extern void irq1(); 
extern void irq11();

struct IDTPtr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

struct IDTEntry {
    uint16_t low_offset;  
    uint16_t sel;           
    uint8_t  always0;      
    uint8_t  flags;         
    uint16_t high_offset;
} __attribute__((packed));


void send_eoi();
void load_idt_register(struct IDTPtr*);
void idt_init();
void set_idt_gate(int n, uint32_t handler);
void load_idt(void* idt_ptr); 

#endif

