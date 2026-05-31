#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

// Syscall numbers
#define SYSCALL_EXIT    0
#define SYSCALL_WRITE   1
#define SYSCALL_READ    2
#define SYSCALL_WAIT    3
#define SYSCALL_SBRK    4

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

// Newlib Stubs
int _write(int file, char *ptr, int len) {
    if (file == 1 || file == 2) {
        syscall(SYSCALL_WRITE, (uint64_t)ptr, (uint64_t)len, 0);
        return len;
    }
    return -1;
}
int write(int file, char *ptr, int len) __attribute__((alias("_write")));

int _read(int file, char *ptr, int len) {
    if (file == 0) {
        return (int)syscall(SYSCALL_READ, (uint64_t)ptr, (uint64_t)len, 0);
    }
    return -1;
}
int read(int file, char *ptr, int len) __attribute__((alias("_read")));

void * _sbrk(ptrdiff_t incr) {
    return (void *)syscall(SYSCALL_SBRK, (uint64_t)incr, 0, 0);
}
void * sbrk(ptrdiff_t incr) __attribute__((alias("_sbrk")));

void _exit(int status) {
    syscall(SYSCALL_EXIT, (uint64_t)status, 0, 0);
    while (1);
}

int _close(int file) { return -1; }
int close(int file) __attribute__((alias("_close")));

int _fstat(int file, struct stat *st) {
    st->st_mode = S_IFCHR;
    return 0;
}
int fstat(int file, struct stat *st) __attribute__((alias("_fstat")));

int _isatty(int file) { return 1; }
int isatty(int file) __attribute__((alias("_isatty")));

int _lseek(int file, int ptr, int dir) { return 0; }
int lseek(int file, int ptr, int dir) __attribute__((alias("_lseek")));

int _getpid() { return 1; }
int getpid() __attribute__((alias("_getpid")));

int _kill(int pid, int sig) { return -1; }
int kill(int pid, int sig) __attribute__((alias("_kill")));
