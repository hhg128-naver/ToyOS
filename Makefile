# 크로스 컴파일러 경로 정의
TOOLCHAIN_DIR = $(shell pwd)/toolchain/install
CLANG         = clang
CLANGXX       = clang++
LD_LLD        = ld.lld
CROSS_OBJCOPY = $(TOOLCHAIN_DIR)/bin/x86_64-elf-objcopy
NASM          = nasm

# 호스트 GCC (UEFI 빌드용)
HOST_GCC      = clang
HOST_LD       = ld.lld
HOST_OBJCOPY  = objcopy

# 라이브러리 파일 경로 직접 정의
LIBC_A     = $(TOOLCHAIN_DIR)/x86_64-elf/lib/libc.a
LIBM_A     = $(TOOLCHAIN_DIR)/x86_64-elf/lib/libm.a
LIBGCC_A   = $(shell $(CLANG) -print-libgcc-file-name)

# 디렉토리 정의
SRC_DIR = src
BOOT_DIR = $(SRC_DIR)/bootloader/legacy
UEFI_DIR = $(SRC_DIR)/bootloader/uefi
KERNEL_DIR = $(SRC_DIR)/kernel
BUILD_DIR = build
KERNEL_BUILD_DIR = $(BUILD_DIR)/kernel
UEFI_BUILD_DIR = $(BUILD_DIR)/uefi

# UEFI 관련 경로 및 설정
EFI_INC     = /usr/include/efi
EFI_LIB     = /usr/lib
EFI_CRT0    = $(EFI_LIB)/crt0-efi-x86_64.o
EFI_LDS     = $(EFI_LIB)/elf_x86_64_efi.lds

# 컴파일 플래그 (64-bit Kernel)
NEWLIB_INC = $(TOOLCHAIN_DIR)/x86_64-elf/include
CFLAGS    = -target x86_64-elf -m64 -c -g -I$(KERNEL_DIR) -I$(NEWLIB_INC) -fno-stack-protector -mno-red-zone -fno-pic -ffreestanding
CXXFLAGS  = $(CFLAGS) -fno-exceptions -fno-rtti
LDFLAGS   = -m elf_x86_64 -T $(KERNEL_DIR)/link.ld

# 컴파일 플래그 (UEFI) - 호스트 GCC용
EFI_CFLAGS = -g -fno-stack-protector -fpic -fshort-wchar -mno-red-zone \
             -I$(EFI_INC) -I$(EFI_INC)/x86_64 -I$(EFI_INC)/protocol
EFI_LDFLAGS = -nostdlib -znocombreloc -T $(EFI_LDS) -shared -Bsymbolic \
              -L$(EFI_LIB) $(EFI_CRT0)

# 소스 파일 및 오브젝트 파일
UEFI_SOURCES = $(wildcard $(UEFI_DIR)/*.c)

CPP_SOURCES  = $(wildcard $(KERNEL_DIR)/*.cpp)
CPP_OBJECTS  = $(patsubst $(KERNEL_DIR)/%.cpp, $(KERNEL_BUILD_DIR)/%.o, $(CPP_SOURCES))
# ap_trampoline.asm은 flat binary(-f bin)로, ap_trampoline_wrapper.asm은
# 이 바이너리를 incbin으로 포함하는 ELF64 오브젝트로 별도 빌드합니다.
ASM_SOURCES  = $(filter-out \
                 $(KERNEL_DIR)/ap_trampoline.asm \
                 $(KERNEL_DIR)/ap_trampoline_wrapper.asm, \
                 $(wildcard $(KERNEL_DIR)/*.asm))
ASM_OBJECTS  = $(patsubst $(KERNEL_DIR)/%.asm, $(KERNEL_BUILD_DIR)/%.o, $(ASM_SOURCES))
UEFI_OBJECTS = $(patsubst $(UEFI_DIR)/%.c, $(UEFI_BUILD_DIR)/%.o, $(UEFI_SOURCES))

# AP 트램펄린 관련 파일
TRAMPOLINE_BIN = $(KERNEL_BUILD_DIR)/ap_trampoline.bin
TRAMPOLINE_OBJ = $(KERNEL_BUILD_DIR)/ap_trampoline_wrapper.o

TARGET  = kernel
UEFI_TARGET = bootx64.efi

all: $(TARGET) uefi user_apps

user_apps:
	$(MAKE) -C user

$(TARGET): $(CPP_OBJECTS) $(ASM_OBJECTS) $(TRAMPOLINE_OBJ)
	$(LD_LLD) $(LDFLAGS) -o $@ $^ $(LIBC_A) $(LIBGCC_A)

# AP 트램펄린: flat binary 빌드 (물리 주소 0x8000용, ORG 0x8000)
$(TRAMPOLINE_BIN): $(KERNEL_DIR)/ap_trampoline.asm
	@mkdir -p $(KERNEL_BUILD_DIR)
	$(NASM) -f bin $< -o $@

# AP 트램펄린 래퍼: flat binary를 .rodata 섹션에 incbin으로 포함
$(TRAMPOLINE_OBJ): $(KERNEL_DIR)/ap_trampoline_wrapper.asm $(TRAMPOLINE_BIN)
	@mkdir -p $(KERNEL_BUILD_DIR)
	$(NASM) -f elf64 $< -o $@

$(KERNEL_BUILD_DIR)/%.o: $(KERNEL_DIR)/%.cpp
	@mkdir -p $(KERNEL_BUILD_DIR)
	$(CLANGXX) $(CXXFLAGS) $< -o $@

$(KERNEL_BUILD_DIR)/%.o: $(KERNEL_DIR)/%.asm
	@mkdir -p $(KERNEL_BUILD_DIR)
	$(NASM) -f elf64 $< -o $@

# UEFI 빌드 규칙
uefi: $(UEFI_TARGET)

$(UEFI_BUILD_DIR)/%.o: $(UEFI_DIR)/%.c
	@mkdir -p $(UEFI_BUILD_DIR)
	$(HOST_GCC) $(EFI_CFLAGS) -c $< -o $@

bootx64.so: $(UEFI_OBJECTS)
	$(HOST_LD) $(EFI_LDFLAGS) $^ -o $@ -lefi -lgnuefi

$(UEFI_TARGET): bootx64.so
	$(HOST_OBJCOPY) -j .text -j .sdata -j .data -j .dynamic -j .dynsym \
	           -j .rel -j .rela -j .reloc --target=efi-app-x86_64 $^ $@

.PHONY: all clean run-uefi

run-uefi-hdd: $(UEFI_TARGET) $(TARGET)
	mkdir -p iso/EFI/BOOT
	cp $(UEFI_TARGET) iso/EFI/BOOT/BOOTX64.EFI
	cp $(TARGET) iso/kernel
	sudo mount -o loop hdd.img mnt
	sudo mkdir -p mnt/EFI/BOOT
	sudo cp $(UEFI_TARGET) mnt/EFI/BOOT/BOOTX64.EFI
	sudo cp $(TARGET) mnt/kernel
	sudo cp user/*.elf mnt/ || true
	sudo umount mnt
	qemu-system-x86_64 \
		-smp 4 \
		-m 512M \
		-bios /usr/share/ovmf/OVMF.fd \
		-drive file=hdd.img,format=raw,if=ide

run-uefi-hdd-debug: $(UEFI_TARGET) $(TARGET)
	mkdir -p iso/EFI/BOOT
	cp $(UEFI_TARGET) iso/EFI/BOOT/BOOTX64.EFI
	cp $(TARGET) iso/kernel
	sudo mount -o loop hdd.img mnt
	sudo mkdir -p mnt/EFI/BOOT
	sudo cp $(UEFI_TARGET) mnt/EFI/BOOT/BOOTX64.EFI
	sudo cp $(TARGET) mnt/kernel
	sudo cp user/*.elf mnt/ || true
	sudo umount mnt
	qemu-system-x86_64 \
		-smp 4 \
		-m 512M \
		-bios /usr/share/ovmf/OVMF.fd \
		-drive file=hdd.img,format=raw,if=ide -s -S

clean:
	rm -rf $(BUILD_DIR) $(TARGET) $(UEFI_TARGET) bootx64.so iso
	$(MAKE) -C user clean
