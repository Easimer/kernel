CC=i686-elf-gcc
CXX=i686-elf-g++
CFLAGS=-ffreestanding -I include/ -m32 -Os -nostdlib
CXXFLAGS=-ffreestanding -I include/ -m32 -Os -nostdlib
PROG_OBJ_C=$(patsubst %.c,%.o,$(wildcard src/*.c))
PROG_OBJ_CPP=$(patsubst %.cpp,%.o,$(wildcard src/*.cpp))
PROG_OBJ=glue.o kernel_sc.o $(PROG_OBJ_C) $(PROG_OBJ_CPP)

all: program.exe

program.exe: $(PROG_OBJ)
	$(CXX) -ffreestanding -nostdlib -Os -m32 -Wl,--nmagic -T linker.ld -o program.exe $(PROG_OBJ) -lgcc
