[GLOBAL irq11]
[EXTERN irq11_handler]

[EXTERN send_eoi]
[EXTERN set_net_packet_flag]

irq11:
    cli
    pusha
    call set_net_packet_flag
    call send_eoi
    popa
    sti
    iretd
