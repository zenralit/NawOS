global load_idt_register

load_idt_register:
    mov eax, [esp + 4]
    lidt [eax]
    ret