[BITS 16]
[ORG 0x7C00]

%ifndef KERNEL_LOAD_SECTORS
%error "KERNEL_LOAD_SECTORS must be defined for the ISO bootloader"
%endif

%define KERNEL_IMAGE_SOURCE    0x7E00
%define KERNEL_PROTECTED_ENTRY 0x10000

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    mov si, msg_load

.print:
    lodsb
    or al, al
    jz .enter_pm
    mov ah, 0x0E
    int 0x10
    jmp .print

.enter_pm:
    in al, 0x92
    or al, 2
    out 0x92, al

    lgdt [gdt_desc]

    mov eax, cr0
    or eax, 1
    mov cr0, eax

    jmp 0x08:protected

gdt_start:
    dq 0
    dq 0x00CF9A000000FFFF
    dq 0x00CF92000000FFFF
gdt_end:

gdt_desc:
    dw gdt_end - gdt_start - 1
    dd gdt_start

[BITS 32]
protected:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000

    ; The El Torito boot image is preloaded at 0x7C00, with the kernel
    ; placed immediately after the boot sector. Copy backwards because the
    ; source range can overlap the kernel's final address at 0x10000.
    mov esi, KERNEL_IMAGE_SOURCE + (KERNEL_LOAD_SECTORS * 512) - 4
    mov edi, KERNEL_PROTECTED_ENTRY + (KERNEL_LOAD_SECTORS * 512) - 4
    mov ecx, (KERNEL_LOAD_SECTORS * 512) / 4
    std
    rep movsd
    cld

    jmp 0x08:KERNEL_PROTECTED_ENTRY

msg_load db "Loading kernel from ISO...", 0

times 510-($-$$) db 0
dw 0xAA55
