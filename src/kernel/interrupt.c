#include "kernel.h"
#include "keyboard.h"

/* 어셈블리에서 정의된 포트 제어 함수 */
extern void outb(uint16_t port, uint8_t data);
extern uint8_t inb(uint16_t port);

/* 어셈블리(isr_common/irq_common)에서 푸시한 레지스터 구조 */
typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rdx, rcx, rbx, rax;
    uint64_t interrupt_number, error_code;
    uint64_t rip, cs, rflags, rsp, ss;
} Registers;

/* PIC 관련 상수 */
#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1
#define PIC_EOI      0x20

/*
 * SendEOI: PIC에게 인터럽트 처리 완료를 알립니다.
 */
void SendEOI(uint64_t irq) {
    if (irq >= 40) {
        outb(PIC2_COMMAND, PIC_EOI);
    }
    outb(PIC1_COMMAND, PIC_EOI);
}

/*
 * ExceptionHandler: 모든 CPU 예외가 이곳으로 모입니다.
 */
void ExceptionHandler(Registers *regs) {
    if (regs->interrupt_number == 3) {
        return;
    }
    
    /* 예외 발생 시 화면 출력 등 추가 로직 가능 */
    while(1);
}

/*
 * InterruptHandler: 모든 하드웨어 인터럽트가 이곳으로 모입니다.
 */
void InterruptHandler(Registers *regs) {
    uint64_t irq = regs->interrupt_number;

    if (irq == 33) {
        Keyboard_Handler();
    }

    /* 처리가 끝나면 반드시 PIC에 EOI 전송 */
    SendEOI(irq);
}
