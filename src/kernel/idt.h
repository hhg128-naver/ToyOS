#ifndef IDT_H
#define IDT_H

#include "kernel.h"

/* IDT 엔트리 구조체 (64비트) */
struct IDTEntry {
    uint16_t isr_low;    // ISR 주소 하위 16비트
    uint16_t kernel_cs;  // 커널 코드 세그먼트 (0x08)
    uint8_t  ist;        // Interrupt Stack Table (0이면 미사용)
    uint8_t  attributes; // 권한 및 타입 (0x8E: Present, Ring 0, Interrupt Gate)
    uint16_t isr_mid;    // ISR 주소 중간 16비트
    uint32_t isr_high;   // ISR 주소 상위 32비트
    uint32_t reserved;   // 예약됨 (0)
} __attribute__((packed));

/* IDT Pointer 구조체 */
struct IDTPtr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

/* IDT 초기화 함수 */
void InitIDT();

/* 어셈블리에서 정의할 IDT 로드 함수 */
extern void LoadIDT(struct IDTPtr *idt_ptr);

/* 기본 예외 핸들러 (어셈블리에서 구현) */
extern void isr0();  // Divide Error
extern void isr3();  // Breakpoint
extern void isr13(); // General Protection Fault
extern void isr14(); // Page Fault

/* 하드웨어 인터럽트 핸들러 (어셈블리에서 구현) */
extern void irq32(); // IRQ 0: Timer
extern void irq33(); // IRQ 1: Keyboard

#endif
