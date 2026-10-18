[GLOBAL irq11]
[EXTERN send_eoi]
[EXTERN network_handle_irq]

irq11:
    cli
    pusha
    call network_handle_irq
    call send_eoi
    popa
    sti
    iretd
