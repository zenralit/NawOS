[BITS 32]
global kernel_entry
extern kernel_main

section .text
kernel_entry:
    cli
    mov esp, 0x90000        
    call kernel_main
.hang:
    hlt
    jmp .hang

