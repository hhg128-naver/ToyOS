#include "kernel.h"
#include "keyboard.h"
#include "task.h"

/* 어셈블리에서 정의된 포트 제어 함수 */
extern void outb(uint16_t port, uint8_t data);
extern uint8_t inb(uint16_t port);

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
uint64_t ExceptionHandler(Context *regs) {
    if (regs->interrupt_number == 3) {
        return (uint64_t)regs;
    }
    
    Printf("\n[CPU EXCEPTION OCCURRED]\n");

    /* 예외 발생 시 일단 현재 태스크 정지 (무한 루프) */
    while(1);
    return (uint64_t)regs;
}

/*
 * InterruptHandler: 모든 하드웨어 인터럽트가 이곳으로 모입니다.
 */
uint64_t InterruptHandler(Context *regs) {
    uint64_t irq = regs->interrupt_number;
    uint64_t next_rsp = (uint64_t)regs;

    if (irq == 32) {
        // 타이머 인터럽트: 스케줄링 수행
        next_rsp = Schedule((uint64_t)regs);
    } else if (irq == 33) {
        Keyboard_Handler();
    }

    /* 처리가 끝나면 반드시 PIC에 EOI 전송 */
    SendEOI(irq);

    return next_rsp;
}
