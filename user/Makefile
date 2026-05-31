CC = clang
LD = ld.lld
AS = nasm

# Newlib paths
NEWLIB_INCLUDE = ../toolchain/install/x86_64-elf/include
NEWLIB_LIB = ../toolchain/install/x86_64-elf/lib
RUNTIME_LIB = $(dir $(shell $(CC) -print-libgcc-file-name))

CFLAGS = -target x86_64-elf -m64 -ffreestanding -fno-stack-protector -mno-red-zone -fno-pic -I$(NEWLIB_INCLUDE)
LDFLAGS = -m elf_x86_64 -Ttext 0x40000000 -e _start -L$(NEWLIB_LIB) -L$(RUNTIME_LIB)

APPS = hello.elf counter.elf

all: $(APPS)

hello.elf: entry.o libnewlib.o hello.o
	$(LD) $(LDFLAGS) $^ -lc -lm -lgcc -o $@

counter.elf: entry.o libnewlib.o counter.o
	$(LD) $(LDFLAGS) $^ -lc -lm -lgcc -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.asm
	$(AS) -f elf64 $< -o $@

clean:
	rm -f *.o *.elf
