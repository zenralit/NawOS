[BITS 16]
[ORG 0x7C00]

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
    mov ah, 0x02        ; функция чтения секторов
    mov al, 40          ; количество секторов
    mov ch, 0           ; цилиндр
    mov cl, 10          ; сектор (начинается с 1)
    mov dh, 0           ; головка
    mov dl, 0x00        ; диск (0x00 для флоппи, 0x80 для HDD)
    mov bx, 0x1000      ; адрес загрузки
    int 0x13            ; BIOS прерывание

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

    jmp 0x08:0x1000

boot_drive db 0
msg_load   db "Loading kernel...",0
msg_err    db "Disk read error!",0

times 510-($-$$) db 0
dw 0xAA55