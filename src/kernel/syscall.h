#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>

/* 시스템 콜 번호 정의 */
#define SYSCALL_EXIT    0
#define SYSCALL_WRITE   1
#define SYSCALL_READ    2
#define SYSCALL_WAIT    3
#define SYSCALL_SBRK    4

/* 시스템 콜 초기화 (MSR 설정 등) */
void InitSyscall();

/* C 언어 시스템 콜 핸들러 */
uint64_t SyscallHandler(uint64_t syscall_num, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5);

#endif
