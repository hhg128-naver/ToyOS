#include "libuser.h"

// Syscall wrapper
static uint64_t syscall(uint64_t num, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
    uint64_t ret;
    __asm__ volatile (
        "mov %1, %%rax\n"
        "mov %2, %%rdi\n"
        "mov %3, %%rsi\n"
        "mov %4, %%rdx\n"
        "syscall\n"
        "mov %%rax, %0\n"
        : "=r"(ret)
        : "r"(num), "r"(arg1), "r"(arg2), "r"(arg3)
        : "rax", "rdi", "rsi", "rdx", "rcx", "r11", "memory"
    );
    return ret;
}

#define SYSCALL_EXIT  0
#define SYSCALL_WRITE 1
#define SYSCALL_WAIT  3

void print(const char* str) {
    syscall(SYSCALL_WRITE, (uint64_t)str, 0, 0);
}

void exit(int code) {
    syscall(SYSCALL_EXIT, (uint64_t)code, 0, 0);
    while (1);
}

void wait(uint64_t pid) {
    syscall(SYSCALL_WAIT, pid, 0, 0);
}

void sleep(int ms) {
    // 임시 delay 구현 (매우 부정확하지만 테스트용)
    for (volatile int i = 0; i < ms * 10000; i++) {
        __asm__ volatile("nop");
    }
}
