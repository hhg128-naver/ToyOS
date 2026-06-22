#include <stdio.h>
#include "kernel.h"
#include "keyboard.h"
#include "mouse.h"
#include "console.h"
#include "task.h"
#include "apic.h"
#include "console.h"




/* PIC 관련 상수 */
#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1
#define PIC_EOI      0x20

/*
 * SendEOI: PIC에게 인터럽트 처리 완료를 알립니다.
 */
void SendEOI(uint64_t irq)
{
    /* 8259A PIC를 비활성화하고 I/O APIC 대칭 모드를 사용하므로, Local APIC에 EOI를 보냅니다. */
    APIC_SendEOI();
}

/*
 * ExceptionHandler: 모든 CPU 예외가 이곳으로 모입니다.
 */
extern "C" uint64_t ExceptionHandler(Context *regs)
{
    if (regs->interrupt_number == 3)
    {
        return (uint64_t)regs;
    }
    
    kPrintf("\n[CPU EXCEPTION OCCURRED] %d\n", (int)regs->interrupt_number);
    static const char* exception_names[] = {
        "#DE Divide Error", "#DB Debug", "NMI Interrupt", "#BP Breakpoint",
        "#OF Overflow", "#BR BOUND Range Exceeded", "#UD Invalid Opcode", "#NM Device Not Available",
        "#DF Double Fault", "Coprocessor Segment Overrun", "#TS Invalid TSS", "#NP Segment Not Present",
        "#SS Stack Segment Fault", "#GP General Protection Fault", "#PF Page Fault", "Reserved",
        "#MF x87 FPU Floating-Point Error", "#AC Alignment Check", "#MC Machine Check", "#XM SIMD Floating-Point Exception",
        "#VE Virtualization Exception", "Control Protection Exception", "Reserved", "Reserved",
        "Reserved", "Reserved", "Reserved", "Reserved",
        "Reserved", "VMM Communication Exception", "Security Exception", "Reserved"
    };

    if (regs->interrupt_number < 32)
    {
        kPrintf("Type: %s\n", exception_names[regs->interrupt_number]);
    }

    kPrintf("Exception No: %d, Error Code: 0x%x\n", (int)regs->interrupt_number, (int)regs->error_code);
    kPrintf("Faulting RIP: %p, RSP: %p\n", (void*)regs->rip, (void*)regs->rsp);

    if (regs->interrupt_number == 14)
    {
        uint64_t cr2;
        __asm__ __volatile__("mov %%cr2, %0" : "=r"(cr2));
        kPrintf("Page Fault Virtual Address (CR2): %p\n", (void*)cr2);
        
        // 에러 코드 분석
        kPrintf("Page Fault Details:\n");
        kPrintf("  Present (P): %s\n", (regs->error_code & 1) ? "Yes (protection violation)" : "No (page not present)");
        kPrintf("  Write (W): %s\n", (regs->error_code & 2) ? "Yes (write access)" : "No (read access)");
        kPrintf("  User (U): %s\n", (regs->error_code & 4) ? "Yes (user mode)" : "No (supervisor mode)");
        kPrintf("  Reserved (RSVD): %s\n", (regs->error_code & 8) ? "Yes (reserved bit violation)" : "No");
        kPrintf("  Instruction Fetch (I/D): %s\n", (regs->error_code & 16) ? "Yes (instruction fetch)" : "No");
        
        if (regs->error_code & 8)
        {
            kPrintf("Cause: Reserved bit violation detected. Page tables contain invalid non-zero reserved bits.\n");
        }
        else if (regs->error_code & 1)
        {
            kPrintf("Cause: Protection violation. Access rights do not match page attributes.\n");
        }
        else
        {
            kPrintf("Cause: Page not present. The requested page is not mapped in memory.\n");
        }
    }
    else if (regs->interrupt_number == 13) // General Protection Fault
    {
        kPrintf("GPF Details:\n");
        if (regs->error_code != 0)
        {
            kPrintf("  Selector Index: %d\n", (int)(regs->error_code >> 3));
            kPrintf("  Table: %s\n", (regs->error_code & 4) ? "LDT" : ((regs->error_code & 2) ? "IDT" : "GDT"));
            kPrintf("  Source: %s\n", 
                (regs->error_code & 1) ? "External" : "Internal"
            );
        }
        else
        {
            kPrintf("Cause: Non-selector related violation (e.g., privilege, canonical address)\n");
        }
    }

    while(1);
    return (uint64_t)regs;
}

/*
 * InterruptHandler: 모든 하드웨어 인터럽트가 이곳으로 모입니다.
 */
extern "C" uint64_t InterruptHandler(Context *regs)
{
    uint64_t irq = regs->interrupt_number;
    uint64_t next_rsp = (uint64_t)regs;
    static uint64_t system_ticks = 0;

    if (irq == 32)
    {
        system_ticks++;

        if (system_ticks % 100 == 0)
        {

            char sec_char = ((system_ticks / 100) % 10) + '0';
            kPutChar(boot_info_global, 50, 10, 'T', 0x0000FF00, 0x00000033);
            kPutChar(boot_info_global, 60, 10, ':', 0x0000FF00, 0x00000033);
            kPutChar(boot_info_global, 70, 10, sec_char, 0x0000FF00, 0x00000033);
        }

        next_rsp = Schedule((uint64_t)regs);
    }
    else if (irq == 48) /* APIC Timer */
    {
        system_ticks++;

        if (system_ticks % 100 == 0)
        {

            char sec_char = ((system_ticks / 100) % 10) + '0';
            kPutChar(boot_info_global, 50, 10, 'T', 0x0000FF00, 0x00000033);
            kPutChar(boot_info_global, 60, 10, ':', 0x0000FF00, 0x00000033);
            kPutChar(boot_info_global, 70, 10, sec_char, 0x0000FF00, 0x00000033);
        }

        next_rsp = Schedule((uint64_t)regs);

        /* APIC Timer는 PIC EOI가 아닌 APIC EOI를 사용 */
        APIC_SendEOI();
        return next_rsp;
    }
    else if (irq == 33)
    {
        Keyboard_Handler();
    }
    else if (irq == 44)
    {
        Mouse_Handler();

        /* 마우스 휠 스크롤 → 콘솔 스크롤백 연결 */
        int8_t scroll = Mouse_GetScroll();
        if (scroll != 0)
        {
            Console_HandleScroll(scroll);
        }
    }

    SendEOI(irq);

    return next_rsp;
}
