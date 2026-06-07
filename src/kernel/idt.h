#ifndef IDT_H
#define IDT_H

#include "kernel.h"

/* IDT 엔트리 구조체 (64비트) */
struct IDTEntry
{
    uint16_t isr_low;    // ISR 주소 하위 16비트
    uint16_t kernel_cs;  // 커널 코드 세그먼트 (0x08)
    uint8_t  ist;        // Interrupt Stack Table (0이면 미사용)
    uint8_t  attributes; // 권한 및 타입 (0x8E: Present, Ring 0, Interrupt Gate)
    uint16_t isr_mid;    // ISR 주소 중간 16비트
    uint32_t isr_high;   // ISR 주소 상위 32비트
    uint32_t reserved;   // 예약됨 (0)
} __attribute__((packed));

/* IDT Pointer 구조체 */
struct IDTPtr
{
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

/* IDT 초기화 함수 */
void InitIDT();

/**
 * LoadIDT_AP: AP에 BSP의 IDT를 로드합니다.
 * ap_entry()에서 호출하여 예외 핸들링을 활성화합니다.
 */
void LoadIDT_AP(void);

/* BSP의 IDT 포인터 (AP에서 접근) */
extern struct IDTPtr idt_ptr;

/* 어셈블리에서 정의할 IDT 로드 함수 */
extern void LoadIDT(struct IDTPtr *idt_ptr);

/* 기본 예외 핸들러 (어셈블리에서 구현) */
extern void isr0(), isr1(), isr2(), isr3(), isr4(), isr5(), isr6(), isr7(), isr8(), isr9();
extern void isr10(), isr11(), isr12(), isr13(), isr14(), isr15(), isr16(), isr17(), isr18(), isr19();
extern void isr20(), isr21(), isr22(), isr23(), isr24(), isr25(), isr26(), isr27(), isr28(), isr29();
extern void isr30(), isr31();

/* 하드웨어 인터럽트 핸들러 (어셈블리에서 구현) */
extern void irq32(); // IRQ 0: Timer
extern void irq33(); // IRQ 1: Keyboard
extern void irq44(); // IRQ 12: Mouse
extern void irq48(); // APIC Timer
extern void irq255(); // LAPIC Spurious Interrupt (벡터 0xFF, EOI 불필요)

#endif
