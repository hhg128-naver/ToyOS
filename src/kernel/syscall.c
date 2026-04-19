#include "syscall.h"
#include "gdt.h"

void InitSyscall() {
    /* 1. EFER의 SCE(System Call Extensions) 비트 활성화 */
    uint64_t efer = ReadMSR(MSR_EFER);
    efer |= 1; // Bit 0: SCE
    WriteMSR(MSR_EFER, efer);

    /* 2. STAR 설정
     * Bits 47:32 - Kernel CS=0x08, SS=0x10 (우리는 0x08을 기록)
     * Bits 63:48 - User CS/SS base. 
     * sysret은 CS=(base+16)|3, SS=(base+8)|3 을 사용함.
     * base를 0x10으로 설정하면 CS=0x23, SS=0x1B가 됨.
     */
    uint64_t star = ((uint64_t)0x08 << 32) | ((uint64_t)0x10 << 48);
    WriteMSR(MSR_STAR, star);

    /* 3. LSTAR 설정 (64비트 진입점) */
    WriteMSR(MSR_LSTAR, (uint64_t)SyscallEntry);

    /* 4. SFMASK 설정 (syscall 호출 시 RFLAGS에서 클리어할 비트들)
     * 인터럽트(Bit 9), 트랩(Bit 8) 등을 마스킹함.
     */
    WriteMSR(MSR_SFMASK, 0x200); // IF 비트 마스킹 (인터럽트 비활성화)
}

/* C로 구현된 시스템 콜 핸들러 */
void SyscallHandler(uint64_t syscall_num, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
    // 임시로 화면에 출력하여 확인
    // Printf("Syscall: %d (%lx, %lx, %lx)\n", syscall_num, arg1, arg2, arg3);
}
