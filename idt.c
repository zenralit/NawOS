#include "idt.h"
#include "ports.h"
#include "keyboard.h"
#include <stdint.h>

#define IDT_ENTRIES 256

struct IDTEntry idt[IDT_ENTRIES];
struct IDTPtr idt_ptr;

//struct idt_entry idt[256];
//struct idt_ptr idt_ptr;

extern void load_idt_register(struct IDTPtr*);
extern void irq1_handler(); 
extern void irq0_handler(); 
extern void irq11_handler();

//set_idt_gate(0x2B, (uint32_t)irq11_handler);

void set_idt_gate(int n, uint32_t handler) {
    idt[n].low_offset = handler & 0xFFFF;
    idt[n].sel = 0x08;               
    idt[n].always0 = 0;
    idt[n].flags = 0x8E;            
    idt[n].high_offset = (handler >> 16) & 0xFFFF;
}

void remap_pic() {
    outb(0x20, 0x11); 
    outb(0xA0, 0x11); 

    outb(0x21, 0x20); 
    outb(0xA1, 0x28); 

    outb(0x21, 0x04); 
    outb(0xA1, 0x02); 

    outb(0x21, 0x01); 
    outb(0xA1, 0x01); 

    outb(0x21, 0xFC); 
    outb(0xA1, 0xFF); 
}

void idt_init()
{
    remap_pic();
    set_idt_gate(32, (uint32_t)irq0); 
    set_idt_gate(33, (uint32_t)irq1); 
    set_idt_gate(43, (uint32_t)irq11);

    idt_ptr.limit = sizeof(idt) - 1;
    idt_ptr.base = (uint32_t)&idt;

    load_idt_register(&idt_ptr);
}

void send_eoi() {
    outb(0x20, 0x20); 
    outb(0xA0, 0x20);
}