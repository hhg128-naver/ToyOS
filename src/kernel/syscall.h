#ifndef SYSCALL_H
#define SYSCALL_H

#include "kernel.h"

/* MSR 주소 정의 */
#define MSR_EFER        0xC0000080
#define MSR_STAR        0xC0000081
#define MSR_LSTAR       0xC0000082
#define MSR_CSTAR       0xC0000083
#define MSR_SFMASK      0xC0000084

/* 시스템 콜 초기화 */
void InitSyscall();

/* 어셈블리로 구현된 시스템 콜 진입점 */
extern void SyscallEntry();

/* MSR 읽기/쓰기 함수 (asm_utils.asm에 정의) */
extern void WriteMSR(uint32_t msr, uint64_t value);
extern uint64_t ReadMSR(uint32_t msr);

#endif
