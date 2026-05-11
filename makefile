AS = nasm
CC = gcc
LD = ld
MKISOFS ?= mkisofs

CFLAGS  = -m32 -ffreestanding -O2 -Wall -I.
LDFLAGS = -m elf_i386
DISK_SECTORS = 2048
KERNEL_START_SECTOR = 1
KERNEL_LOAD_SECTORS = 128
ISO_ROOT = build/iso-root
ISO_BOOT_IMAGE = $(ISO_ROOT)/boot/eltorito.img
ISO_IMAGE = nawos.iso
VBOX_DISK_IMAGE = nawos.vdi

OBJS = \
boot/kernel_entry.o \
kernel/interrupts/irq0.o \
kernel/interrupts/irq1.o \
kernel/interrupts/irq11.o \
kernel/interrupts/idt_load.o \
kernel/interrupts/idt.o \
kernel/kernel.o \
kernel/memory/memory.o \
kernel/input/keyboard_driver.o \
kernel/input/input.o \
kernel/shell/shell.o \
kernel/shell/shell_input.o \
kernel/shell/shell_parser.o \
kernel/shell/shell_commands.o \
kernel/terminal/terminal.o \
apps/editor/editor.o \
apps/editor/editor_buffer.o \
apps/editor/editor_cursor.o \
apps/editor/editor_render.o \
apps/editor/editor_file.o \
apps/editor/editor_input.o \
apps/calc/calc.o \
drivers/vga/vga.o \
drivers/screen/screen.o \
drivers/keyboard/keyboard.o \
drivers/ports/ports.o \
drivers/ata/ata.o \
drivers/pci/pci.o \
drivers/rtl8139/rtl8139.o \
fs/nawfs.o \
lib/math.o \
lib/nawstring.o \
lib/nawutil.o \
net/net.o \
net/ip.o \
net/dhcp.o \
net/ethernet.o \
net/ipv4.o \
net/udp.o \
apps/nawlang/parser.o \
apps/nawlang/vm.o \
apps/nawlang/compiler.o 

.PHONY: all iso vbox run run-iso clean

all: os-image.bin

boot/%.o: boot/%.asm
	$(AS) -f elf32 $< -o $@

kernel/interrupts/%.o: kernel/interrupts/%.asm
	$(AS) -f elf32 $< -o $@

kernel/interrupts/idt_load.o: kernel/interrupts/idt_load.asm
	$(AS) -f elf32 $< -o $@

boot/bootloader.bin: boot/bootloader.asm
	$(AS) -f bin $< -o $@

boot/bootloader_iso.bin: boot/bootloader_iso.asm kernel.raw
	@kernel_bytes=$$(wc -c < kernel.raw); \
	kernel_sectors=$$(( ($$kernel_bytes + 511) / 512 )); \
	$(AS) -DKERNEL_LOAD_SECTORS=$$kernel_sectors -f bin $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

kernel.bin: $(OBJS) boot/linker.ld
	$(LD) $(LDFLAGS) -T boot/linker.ld -o $@ $(OBJS)

kernel.raw: kernel.bin
	objcopy -O binary $< $@

os-image.bin: boot/bootloader.bin kernel.raw
	@kernel_bytes=$$(wc -c < kernel.raw); \
	kernel_sectors=$$(( ($$kernel_bytes + 511) / 512 )); \
	if [ $$kernel_sectors -gt $(KERNEL_LOAD_SECTORS) ]; then \
		echo "kernel.raw is too large: $$kernel_sectors sectors, limit is $(KERNEL_LOAD_SECTORS)"; \
		exit 1; \
	fi; \
	rm -f $@; \
	truncate -s $$(( $(DISK_SECTORS) * 512 )) $@; \
	dd if=boot/bootloader.bin of=$@ conv=notrunc bs=512 seek=0 count=1 status=none; \
	dd if=kernel.raw of=$@ conv=notrunc bs=512 seek=$(KERNEL_START_SECTOR) status=none

$(ISO_BOOT_IMAGE): boot/bootloader_iso.bin kernel.raw
	@kernel_bytes=$$(wc -c < kernel.raw); \
	kernel_sectors=$$(( ($$kernel_bytes + 511) / 512 )); \
	total_sectors=$$(( $$kernel_sectors + 1 )); \
	mkdir -p $(dir $@); \
	rm -f $@; \
	truncate -s $$(( $$total_sectors * 512 )) $@; \
	dd if=boot/bootloader_iso.bin of=$@ conv=notrunc bs=512 seek=0 count=1 status=none; \
	dd if=kernel.raw of=$@ conv=notrunc bs=512 seek=1 status=none

$(ISO_IMAGE): $(ISO_BOOT_IMAGE)
	@total_sectors=$$(( $$(wc -c < $(ISO_BOOT_IMAGE)) / 512 )); \
	rm -f $@; \
	$(MKISOFS) -quiet -V NAWOS -o $@ \
		-b boot/eltorito.img \
		-c boot/boot.cat \
		-no-emul-boot \
		-boot-load-size $$total_sectors \
		$(ISO_ROOT)

iso: $(ISO_IMAGE)

$(VBOX_DISK_IMAGE): os-image.bin
	VBoxManage convertfromraw $< $@ --format VDI >/dev/null

vbox: $(ISO_IMAGE) $(VBOX_DISK_IMAGE)

run: os-image.bin
	qemu-system-i386 \
	-drive file=os-image.bin,format=raw,if=ide,index=0 \
	-netdev user,id=net0 \
	-device rtl8139,netdev=net0 \
	-serial mon:stdio

run-iso: $(ISO_IMAGE)
	qemu-system-i386 \
		-cdrom $(ISO_IMAGE) \
		-boot d \
		-netdev user,id=net0 \
		-device rtl8139,netdev=net0 \
		-serial mon:stdio

clean:
	rm -f \
	boot/*.o kernel/*.o kernel/*/*.o kernel/*/*/*.o \
	drivers/*/*.o drivers/*/*/*.o \
	fs/*.o lib/*.o nawlang/*.o apps/*/*.o net/*.o \
	boot/bootloader_iso.bin \
	kernel.raw \
	*.bin \
	$(ISO_IMAGE) \
	$(VBOX_DISK_IMAGE); \
	rm -rf $(ISO_ROOT)
