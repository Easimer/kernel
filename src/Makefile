CC=i686-elf-gcc
CXX=i686-elf-g++
CCFLAGS=-g -fno-omit-frame-pointer -std=gnu99 -ffreestanding -O0 -Wall -Wextra -Werror -fpic -fPIE
CXXFLAGS=-g -fno-omit-frame-pointer -std=c++17 -ffreestanding -O0 -Wall -Wextra -Werror -fno-exceptions -fno-rtti -fpic -fPIE -fstack-protector

CRTBEGIN_OBJ:=$(shell $(CC) $(CFLAGS) -print-file-name=crtbegin.o)
CRTEND_OBJ:=$(shell $(CC) $(CFLAGS) -print-file-name=crtend.o)

VERSION=0.2
KERNEL_FILENAME=kernel-$(VERSION).img
KERNEL_CRT=crti.S.o crtn.S.o
KERNEL_CORE_OBJECTS=boot.S.o main.cpp.o logging.cpp.o port_io.S.o multiboot2.cpp.o utils.cpp.o memory.cpp.o simd.S.o exec.cpp.o pfalloc.cpp.o vm.cpp.o
KERNEL_DRIVER_CORE_OBJECTS=pci.cpp.o interrupts.cpp.o interrupts.S.o disk.cpp.o volumes.cpp.o
KERNEL_DRIVER_OBJECTS=pc_vga.cpp.o uart.cpp.o timer.cpp.o ide.cpp.o fat32.cpp.o ps2.cpp.o ps2_keyboard.cpp.o dev_fs.cpp.o
KERNEL_OBJECTS=$(KERNEL_CORE_OBJECTS) $(KERNEL_DRIVER_CORE_OBJECTS) $(KERNEL_DRIVER_OBJECTS)

all: $(KERNEL_FILENAME) boot.iso

$(KERNEL_FILENAME): $(KERNEL_OBJECTS) linker.ld $(KERNEL_CRT)
	$(CXX) -T linker.ld -o $(KERNEL_FILENAME) -ffreestanding -nostdlib crti.S.o $(CRTBEGIN_OBJ) $(KERNEL_OBJECTS) $(CRTEND_OBJ) crtn.S.o -lgcc

%.S.o: %.S
	nasm -Wall -f elf32 -o $@ $<

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
	rm -rf $(KERNEL_FILENAME) $(KERNEL_OBJECTS) $(KERNEL_CRT) boot.iso grub/isoroot/

run: boot.iso
	qemu-system-i386 -cdrom boot.iso -m 64M -boot d -serial stdio $(QFLAGS)
