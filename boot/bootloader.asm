[BITS 16]
[ORG 0x7C00]

%define KERNEL_LOAD_SECTORS 128
%define KERNEL_LOAD_SEGMENT 0x1000
%define KERNEL_LOAD_OFFSET 0x0000
%define KERNEL_PROTECTED_ENTRY 0x10000

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    mov [boot_drive], dl
    mov si, msg_load

.print:
    lodsb
    or al, al
    jz .load
    mov ah,0x0E
    int 0x10
    jmp .print
                            
.load:
    mov si, dap
    mov dl, [boot_drive]
    mov ah, 0x42
    int 0x13
    jc .disk_error

    in al,0x92
    or al,2
    out 0x92,al
    
    lgdt [gdt_desc]
  
    mov eax,cr0
    or eax,1
    mov cr0,eax
    
    jmp 0x08:protected

.disk_error:
    mov si, msg_err

.err:
    lodsb
    or al,al
    jz $
    mov ah,0x0E
    int 0x10
    jmp .err

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
    mov ax,0x10
    mov ds,ax
    mov es,ax
    mov fs,ax
    mov gs,ax
    mov ss,ax
    mov esp,0x90000

    jmp 0x08:KERNEL_PROTECTED_ENTRY

boot_drive db 0
dap:
    db 0x10
    db 0
    dw KERNEL_LOAD_SECTORS
    dw KERNEL_LOAD_OFFSET
    dw KERNEL_LOAD_SEGMENT
    dq 0x0000000000000001
msg_load   db "Loading kernel...",0
msg_err    db "Disk read error!",0

times 510-($-$$) db 0
dw 0xAA55
