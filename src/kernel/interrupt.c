#include "kernel.h"

/* 어셈블리(isr_common)에서 푸시한 레지스터 구조 */
typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rdx, rcx, rbx, rax;
    uint64_t interrupt_number, error_code;
    uint64_t rip, cs, rflags, rsp, ss;
} Registers;

/*
 * ExceptionHandler: 모든 CPU 예외가 이곳으로 모입니다.
 */
void ExceptionHandler(Registers *regs) {
    /* 배경을 빨간색으로 변경하여 치명적 오류 표시 */
    // ClearScreen(boot_info_global, 0x00FF0000); // boot_info 전역화 필요

    if (regs->interrupt_number == 3) {
        /* Breakpoint는 예외적으로 처리 후 복귀 가능 */
        // PrintString(boot_info_global, "EXCEPTION: Breakpoint Hit!\n", 0x00FFFFFF);
        return;
    }

    /* 기타 예외 발생 시 시스템 중단 메시지 (추후 구현) */
    while(1);
}
