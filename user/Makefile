CC = clang
LD = ld.lld
AS = nasm

CFLAGS = -target x86_64-elf -m64 -ffreestanding -fno-stack-protector -mno-red-zone -fno-pic
LDFLAGS = -m elf_x86_64 -Ttext 0x40000000 -e _start

APPS = hello.elf counter.elf

all: $(APPS)

hello.elf: entry.o libuser.o hello.o
	$(LD) $(LDFLAGS) $^ -o $@

counter.elf: entry.o libuser.o counter.o
	$(LD) $(LDFLAGS) $^ -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.asm
	$(AS) -f elf64 $< -o $@

clean:
	rm -f *.o *.elf
