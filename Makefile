# 컴파일러 및 링커 정의
NASM    = nasm
GCC     = gcc
LD      = ld
OBJCOPY = objcopy

# 디렉토리 정의
SRC_DIR = src
BOOT_DIR = $(SRC_DIR)/bootloader/legacy
UEFI_DIR = $(SRC_DIR)/bootloader/uefi
KERNEL_DIR = $(SRC_DIR)/kernel

# UEFI 관련 경로 및 설정
EFI_INC     = /usr/include/efi
EFI_LIB     = /usr/lib
EFI_CRT0    = $(EFI_LIB)/crt0-efi-x86_64.o
EFI_LDS     = $(EFI_LIB)/elf_x86_64_efi.lds

# 컴파일 플래그 (64-bit Kernel) - boot.asm 제외
CFLAGS    = -m64 -c -I$(KERNEL_DIR) -fno-stack-protector -mno-red-zone -fno-pic -ffreestanding -nostdlib
LDFLAGS   = -m elf_x86_64 -T $(KERNEL_DIR)/link.ld -nostdlib

# 컴파일 플래그 (UEFI)
EFI_CFLAGS = -fno-stack-protector -fpic -fshort-wchar -mno-red-zone \
             -I$(EFI_INC) -I$(EFI_INC)/x86_64 -I$(EFI_INC)/protocol
EFI_LDFLAGS = -nostdlib -znocombreloc -T $(EFI_LDS) -shared -Bsymbolic \
              -L$(EFI_LIB) $(EFI_CRT0)

# 소스 파일 및 오브젝트 파일 (ASM_SOURCES 제외)
C_SOURCES   = $(wildcard $(KERNEL_DIR)/*.c)
UEFI_SOURCES = $(wildcard $(UEFI_DIR)/*.c)

C_OBJECTS   = $(patsubst $(KERNEL_DIR)/%.c, $(KERNEL_DIR)/%.o, $(C_SOURCES))
UEFI_OBJECTS = $(patsubst $(UEFI_DIR)/%.c, $(UEFI_DIR)/%.o, $(UEFI_SOURCES))

TARGET  = kernel
UEFI_TARGET = bootx64.efi

all: $(TARGET) uefi

$(TARGET): $(C_OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $^

$(KERNEL_DIR)/%.o: $(KERNEL_DIR)/%.c
	$(GCC) $(CFLAGS) $< -o $@

# UEFI 빌드 규칙
uefi: $(UEFI_TARGET)

$(UEFI_DIR)/%.o: $(UEFI_DIR)/%.c
	$(GCC) $(EFI_CFLAGS) -c $< -o $@

bootx64.so: $(UEFI_OBJECTS)
	$(LD) $(EFI_LDFLAGS) $^ -o $@ -lefi -lgnuefi

$(UEFI_TARGET): bootx64.so
	$(OBJCOPY) -j .text -j .sdata -j .data -j .dynamic -j .dynsym \
	           -j .rel -j .rela -j .reloc --target=efi-app-x86_64 $^ $@

.PHONY: all clean run-uefi

run-uefi: $(UEFI_TARGET) $(TARGET)
	mkdir -p iso/EFI/BOOT
	cp $(UEFI_TARGET) iso/EFI/BOOT/BOOTX64.EFI
	cp $(TARGET) iso/kernel
	qemu-system-x86_64 -bios /usr/share/ovmf/OVMF.fd -drive format=raw,file=fat:rw:iso

clean:
	rm -f $(C_OBJECTS) $(UEFI_OBJECTS) $(TARGET) $(UEFI_TARGET) bootx64.so
	rm -rf iso