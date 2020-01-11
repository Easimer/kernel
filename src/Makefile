CC=i686-elf-gcc
CXX=i686-elf-g++
CCFLAGS=-std=gnu99 -ffreestanding -O2 -Wall -Wextra
CXXFLAGS=-std=c++17 -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-rtti
VERSION=0.1
KERNEL_FILENAME=kernel-$(VERSION).img

KERNEL_OBJECTS=boot.S.o main.cpp.o

all: $(KERNEL_FILENAME) boot.iso

$(KERNEL_FILENAME): $(KERNEL_OBJECTS) linker.ld
	$(CXX) -T linker.ld -o $(KERNEL_FILENAME) -ffreestanding -nostdlib $(KERNEL_OBJECTS) -lgcc

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

.PHONY: clean

clean:
	rm -rf $(KERNEL_FILENAME) $(KERNEL_OBJECTS) boot.iso grub/isoroot/
