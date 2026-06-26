#include "syscall.h"
#include "kernel.h"
#include "gdt.h"
#include "task.h"
#include "keyboard.h"
#include "vmm.h"
#include "pmm.h"
#include "console.h"

/* 어셈블리 함수 선언 (asm_utils.asm) */
extern "C" void WriteMSR(uint32_t msr, uint64_t value);
extern "C" uint64_t ReadMSR(uint32_t msr);
extern "C" void SyscallEntry();

/* MSR 주소 정의 */
#define MSR_STAR 0xC0000081
#define MSR_LSTAR 0xC0000082
#define MSR_SFMASK 0xC0000084
#define MSR_EFER 0xC0000080

/**
 * InitSyscall: syscall/sysret 명령어를 위한 CPU MSR 설정
 */
void InitSyscall()
{
    /* 1. EFER MSR에서 SCE (System Call Extensions) 비트 활성화 */
    uint64_t efer = ReadMSR(MSR_EFER);
    WriteMSR(MSR_EFER, efer | 0x01);

    /* 2. STAR MSR 설정
     * [63:48]: User Base Selector.
     *          sysret은 (Base + 16)을 CS로, (Base + 8)을 SS로 로드.
     *          GDT Index 3: UData(0x18), Index 4: UCode(0x20).
     *          따라서 Base를 Index 2 (0x10)로 설정.
     *
	  비트: 63           48 47                 32 31                             0
	 ┌─────────────────────┬─────────────────────┬──────────────────────────────┐
	 │  SYSRET CS/SS Base  │  SYSCALL CS/SS Base │   32-bit SYSCALL ESP Target  │
	 │      (16bit)        │       (16bit)       │            (32bit)           │
	 └─────────────────────┴─────────────────────┴──────────────────────────────┘
    */
    uint64_t star = ((uint64_t)0x10 << 48) | ((uint64_t)0x08 << 32);
    WriteMSR(MSR_STAR, star);

    /* 3. LSTAR MSR 설정: 64비트 시스템 콜 진입점 주소 등록 */
    WriteMSR(MSR_LSTAR, (uint64_t)SyscallEntry);

    /* 4. SFMASK MSR 설정: syscall 호출 시 자동으로 클리어할 RFLAGS 비트 마스크 */
    WriteMSR(MSR_SFMASK, 0x200 | 0x400);

    kPrintf("Syscall Infrastructure Initialized.\n");
}

/**
 * SyscallHandler: C 언어 시스템 콜 처리기
 */
extern "C" uint64_t SyscallHandler(uint64_t syscall_num, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5)
{
    switch (syscall_num)
    {
    case SYSCALL_WRITE:
    {
        /* arg1: 문자열 주소, arg2: 길이 */
        const char *str = (const char *)arg1;
        uint64_t len = arg2;
        if (len == 0)
        {
            kPrintf(str);
        }
        else
        {
            PrintStringLen(boot_info_global, str, (uint32_t)len, 0x00FFFFFF);
        }
        return len;
    }

    case SYSCALL_READ:
    {
        /* arg1: 버퍼 주소, arg2: 길이 */
        char *buf = (char *)arg1;
        uint64_t len = arg2;
        uint64_t i = 0;
        for (i = 0; i < len; i++)
        {
            buf[i] = Keyboard_GetChar();
            if (buf[i] == '\n')
            {
                i++;
                break;
            }
        }
        return i;
    }

    case SYSCALL_EXIT:
        kPrintf("\n[Kernel] User Task Exited.\n");
        ExitCurrentTask();
        return 0;

    case SYSCALL_WAIT:
        /* arg1: 대기할 태스크 ID */
        WaitTask(arg1);
        return 0;

    case SYSCALL_SBRK:
    {
        /* arg1: 증가량 */
        int64_t increment = (int64_t)arg1;
        Task *current = GetCurrentTask();
        uint64_t old_heap_end = current->heap_end;
        uint64_t new_heap_end = old_heap_end + increment;

        if (increment > 0)
        {
            // 페이지 경계에 맞춰 할당
            uint64_t start_page = (old_heap_end + 0xFFF) & ~0xFFFULL;
            uint64_t end_page = (new_heap_end + 0xFFF) & ~0xFFFULL;
            for (uint64_t addr = start_page; addr < end_page; addr += 0x1000)
            {
                void *phys = PMM_AllocPage();
                if (phys)
                {
                    VMM_MapPageEx((pt_entry *)current->pml4, (void *)addr, phys, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
                }
            }
        }
        current->heap_end = new_heap_end;
        return old_heap_end;
    }

    default:
        return (uint64_t)-1;
    }
}
