#include "idt.h"

static struct IDTEntry idt[256];
static struct IDTPtr idt_ptr;

void SetIDTEntry(uint8_t vector, void* isr, uint8_t flags) {
    struct IDTEntry* entry = &idt[vector];

    uint64_t addr = (uint64_t)isr;
    entry->isr_low    = addr & 0xFFFF;
    entry->kernel_cs  = 0x08; // Kernel Code Segment Index
    entry->ist        = 0;
    entry->attributes = flags;
    entry->isr_mid    = (addr >> 16) & 0xFFFF;
    entry->isr_high   = (addr >> 32) & 0xFFFFFFFF;
    entry->reserved   = 0;
}

void InitIDT() {
    /* 모든 엔트리를 0으로 초기화 */
    for (int i = 0; i < 256; i++) {
        SetIDTEntry(i, 0, 0);
    }

    /* 주요 예외 핸들러 등록 */
    SetIDTEntry(0,  isr0,  0x8E); // Divide Error
    SetIDTEntry(3,  isr3,  0x8E); // Breakpoint
    SetIDTEntry(13, isr13, 0x8E); // General Protection Fault
    SetIDTEntry(14, isr14, 0x8E); // Page Fault

    /* 하드웨어 인터럽트 핸들러 등록 (벡터 32번부터) */
    SetIDTEntry(32, irq32, 0x8E); // IRQ 0: Timer
    SetIDTEntry(33, irq33, 0x8E); // IRQ 1: Keyboard

    /* IDT Pointer 설정 */
    idt_ptr.limit = (sizeof(struct IDTEntry) * 256) - 1;
    idt_ptr.base  = (uint64_t)&idt;

    /* IDT 로드 (어셈블리 함수 호출) */
    LoadIDT(&idt_ptr);
}
