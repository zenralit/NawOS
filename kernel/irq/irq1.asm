[BITS 32]

global irq0
global irq1

extern dummy_timer_callback
extern keyboard_handle_interrupt

irq0:
    cli
    pusha
    call dummy_timer_callback
    popa
    mov al, 0x20
    out 0x20, al
    sti
    iretd

irq1:
    cli
    pusha
    call keyboard_handle_interrupt     
    popa
    mov al, 0x20
    out 0x20, al
    sti
    iretd