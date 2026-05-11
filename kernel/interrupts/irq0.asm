[BITS 32]
global irq0_handler
extern dummy_timer_callback

irq0_handler:
    pusha
    call dummy_timer_callback
    popa
    mov al, 0x20
    out 0x20, al
    iret
