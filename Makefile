# 컴파일러 및 링커 정의
NASM    = nasm
GCC     = gcc
LD      = ld

# 컴파일 플래그
NASMFLAGS = -f elf32
CFLAGS    = -m32 -c
LDFLAGS   = -m elf_i386 -T link.ld

# 소스 파일 및 오브젝트 파일
ASM_SOURCES = $(wildcard *.asm)
C_SOURCES   = $(wildcard *.c)

ASM_OBJECTS = $(patsubst %.asm, %.o, $(ASM_SOURCES))
C_OBJECTS   = $(patsubst %.c, %.o, $(C_SOURCES))

ALL_OBJECTS = $(ASM_OBJECTS) $(C_OBJECTS)

TARGET  = kernel

all: prepare $(TARGET)

prepare: Dependency.dep

$(TARGET): $(ALL_OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $^

%.o: %.asm
	$(NASM) $(NASMFLAGS) $< -o $@

%.o: %.c
	$(GCC) $(CFLAGS) $< -o $@

Dependency.dep: $(C_SOURCES)
	$(GCC) -MM $(C_SOURCES) > $@

ifeq (Dependency.dep, $(wildcard Dependency.dep))
include Dependency.dep
endif

.PHONY: all clean

clean:
	rm -f $(ALL_OBJECTS) $(TARGET) Dependency.dep