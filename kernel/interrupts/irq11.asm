[GLOBAL irq11]
[EXTERN send_eoi]
[EXTERN rtl8139_handle_irq]

irq11:
    cli
    pusha
    call rtl8139_handle_irq
    call send_eoi
    popa
    sti
    iretd
