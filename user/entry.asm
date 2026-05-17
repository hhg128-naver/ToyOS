[BITS 64]
global _start
extern main
extern exit

section .text
_start:
    ; rdi contains 'arg' from the kernel
    call main
    
    ; exit with return value of main
    mov rdi, rax
    call exit

    ; should never reach here
.hang:
    jmp .hang
