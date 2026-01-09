AS = nasm
CC = gcc
LD = ld

CFLAGS  = -m32 -ffreestanding -O2 -Wall -I.
LDFLAGS = -m elf_i386

OBJS = \
boot/kernel_entry.o \
kernel/irq/irq0.o \
kernel/irq/irq1.o \
kernel/irq/irq11.o \
kernel/idt_load.o \
kernel/idt.o \
kernel/kernel.o \
drivers/screen/screen.o \
drivers/keyboard/keyboard.o \
drivers/ports/ports.o \
drivers/disk/disk.o \
drivers/net/pci.o \
drivers/net/rtl8139.o \
drivers/net/net.o \
drivers/net/ip.o \
drivers/net/dhcp.o \
fs/nawfs.o \
lib/math.o \
nawlang/parser.o \
nawlang/vm.o \
nawlang/compiler.o 

all: os-image.bin

boot/%.o: boot/%.asm
	$(AS) -f elf32 $< -o $@

kernel/irq/%.o: kernel/irq/%.asm
	$(AS) -f elf32 $< -o $@

kernel/idt_load.o: kernel/idt_load.asm
	$(AS) -f elf32 $< -o $@

boot/bootloader.bin: boot/bootloader.asm
	$(AS) -f bin $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

kernel.bin: $(OBJS) boot/linker.ld
	$(LD) $(LDFLAGS) -T boot/linker.ld -o $@ $(OBJS)

os-image.bin: boot/bootloader.bin kernel.bin
	cat boot/bootloader.bin kernel.bin > $@

run: os-image.bin
	qemu-system-i386 \
	-drive file=os-image.bin,format=raw,index=0,if=floppy \
	-drive file=fs/nawfs.img,format=raw,if=ide,bus=0,unit=0 \
	-netdev user,id=net0 \
	-device rtl8139,netdev=net0 \
	-serial mon:stdio

clean:
	rm -f \
	boot/*.o kernel/*.o kernel/irq/*.o \
	drivers/*/*.o drivers/*/*/*.o \
	fs/*.o lib/*.o nawlang/*.o \
	*.bin