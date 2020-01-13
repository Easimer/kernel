CC=i686-elf-gcc
CXX=i686-elf-g++
CCFLAGS=-std=gnu99 -ffreestanding -O2 -Wall -Wextra
CXXFLAGS=-std=c++17 -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-rtti

CRTBEGIN_OBJ:=$(shell $(CC) $(CFLAGS) -print-file-name=crtbegin.o)
CRTEND_OBJ:=$(shell $(CC) $(CFLAGS) -print-file-name=crtend.o)

VERSION=0.1
KERNEL_FILENAME=kernel-$(VERSION).img
KERNEL_CORE_OBJECTS=boot.S.o main.cpp.o logging.cpp.o port_io.S.o multiboot2.cpp.o utils.cpp.o memory.cpp.o simd.S.o
KERNEL_DRIVER_CORE_OBJECTS=pci.cpp.o interrupts.cpp.o interrupts.S.o disk.cpp.o volumes.cpp.o
KERNEL_DRIVER_OBJECTS=pc_vga.cpp.o uart.cpp.o timer.cpp.o ide.cpp.o fat32.cpp.o
KERNEL_OBJECTS=$(KERNEL_CORE_OBJECTS) $(KERNEL_DRIVER_CORE_OBJECTS) $(KERNEL_DRIVER_OBJECTS)

all: $(KERNEL_FILENAME) boot.iso

$(KERNEL_FILENAME): $(KERNEL_OBJECTS) linker.ld
	$(CXX) -T linker.ld -o $(KERNEL_FILENAME) -ffreestanding -nostdlib crti.S.o $(CRTBEGIN_OBJ) $(KERNEL_OBJECTS) $(CRTEND_OBJ) crtn.S.o crtn.S.o -lgcc

%.S.o: %.S
	nasm -f elf32 -o $@ $<

%.cpp.o: %.cpp
	$(CXX) $(CXXFLAGS) -o $@ -c $<

%.c.o: %.cpp
	$(CC) $(CFLAGS) -o $@ -c $<

boot.iso: $(KERNEL_FILENAME) grub/isoroot/boot/grub/grub.cfg
	cp $(KERNEL_FILENAME) grub/isoroot/boot/
	grub2-mkrescue -o boot.iso grub/isoroot/

grub/isoroot/boot/grub/grub.cfg: grub/create-grubcfg.sh
	mkdir -p grub/isoroot/boot/grub/
	grub/create-grubcfg.sh grub/isoroot/boot/grub/grub.cfg $(VERSION)

.PHONY: clean run

clean:
	rm -rf $(KERNEL_FILENAME) $(KERNEL_OBJECTS) boot.iso grub/isoroot/

run: boot.iso
	qemu-system-i386 -cdrom boot.iso -m 64M -boot d -serial stdio $(QFLAGS)
