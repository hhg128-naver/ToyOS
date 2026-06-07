;;; ap_trampoline_wrapper.asm
;;; AP 트램펄린 바이너리를 ELF64 커널 오브젝트에 임베드합니다.
;;;
;;; 빌드 방법:
;;;   1. nasm -f bin ap_trampoline.asm -o build/kernel/ap_trampoline.bin
;;;   2. nasm -f elf64 ap_trampoline_wrapper.asm -o build/kernel/ap_trampoline_wrapper.o
;;;
;;; C 코드에서는 다음과 같이 접근합니다:
;;;   extern uint8_t ap_trampoline_bin_start[];
;;;   extern uint8_t ap_trampoline_bin_end[];
;;;   size_t size = ap_trampoline_bin_end - ap_trampoline_bin_start;
;;;   memcpy((void*)0x8000, ap_trampoline_bin_start, size);

[BITS 64]

section .rodata

global ap_trampoline_bin_start
global ap_trampoline_bin_end

ap_trampoline_bin_start:
    incbin "build/kernel/ap_trampoline.bin"
ap_trampoline_bin_end:
