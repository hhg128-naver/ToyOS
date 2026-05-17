#include "syscall.h"
#include "kernel.h"
#include "gdt.h"

/* 어셈블리 함수 선언 (asm_utils.asm) */
extern void WriteMSR(uint64_t msr, uint64_t value);
extern uint64_t ReadMSR(uint64_t msr);
extern void SyscallEntry();

/* MSR 주소 정의 */
#define MSR_STAR            0xC0000081
#define MSR_LSTAR           0xC0000082
#define MSR_SFMASK          0xC0000084
#define MSR_EFER            0xC0000080

/**
 * InitSyscall: syscall/sysret 명령어를 위한 CPU MSR 설정
 */
void InitSyscall() {
    /* 1. EFER MSR에서 SCE (System Call Extensions) 비트 활성화 */
    uint64_t efer = ReadMSR(MSR_EFER);
    WriteMSR(MSR_EFER, efer | 0x01); 

    /* 2. STAR MSR 설정
     * [63:48]: User Base Selector. 
     *          sysret은 (Base + 16)을 CS로, (Base + 8)을 SS로 로드.
     *          GDT Index 3: UData(0x18), Index 4: UCode(0x20). 
     *          따라서 Base를 Index 2 (0x10)로 설정. 
     */
    uint64_t star = ((uint64_t)0x10 << 48) | ((uint64_t)0x08 << 32);
    WriteMSR(MSR_STAR, star);

    /* 3. LSTAR MSR 설정: 64비트 시스템 콜 진입점 주소 등록 */
    WriteMSR(MSR_LSTAR, (uint64_t)SyscallEntry);

    /* 4. SFMASK MSR 설정: syscall 호출 시 자동으로 클리어할 RFLAGS 비트 마스크 */
    WriteMSR(MSR_SFMASK, 0x200 | 0x400); 

    Printf("Syscall Infrastructure Initialized.\n");
}

/**
 * SyscallHandler: C 언어 시스템 콜 처리기
 */
uint64_t SyscallHandler(uint64_t syscall_num, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    switch (syscall_num) {
        case SYSCALL_WRITE:
            /* arg1: 문자열 주소 */
            Printf((const char*)arg1);
            return 0;

        case SYSCALL_EXIT:
            Printf("\n[Kernel] User Task Exited.\n");
            extern void ExitCurrentTask();
            ExitCurrentTask();
            return 0;

        default:
            return (uint64_t)-1;
    }
}
