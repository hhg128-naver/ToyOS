#include "idt.h"

static struct IDTEntry idt[256];
static struct IDTPtr idt_ptr;

void SetIDTEntry(uint8_t vector, void* isr, uint8_t flags)
{
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

void InitIDT()
{
    /* 모든 엔트리를 0으로 초기화 */
    for (int i = 0; i < 256; i++)
    {
        SetIDTEntry(i, 0, 0);
    }

    /* 모든 예외 핸들러 등록 (0~31) */
    SetIDTEntry(0,  isr0,  0x8E);
    SetIDTEntry(1,  isr1,  0x8E);
    SetIDTEntry(2,  isr2,  0x8E);
    SetIDTEntry(3,  isr3,  0x8E);
    SetIDTEntry(4,  isr4,  0x8E);
    SetIDTEntry(5,  isr5,  0x8E);
    SetIDTEntry(6,  isr6,  0x8E);
    SetIDTEntry(7,  isr7,  0x8E);
    SetIDTEntry(8,  isr8,  0x8E);
    SetIDTEntry(9,  isr9,  0x8E);
    SetIDTEntry(10, isr10, 0x8E);
    SetIDTEntry(11, isr11, 0x8E);
    SetIDTEntry(12, isr12, 0x8E);
    SetIDTEntry(13, isr13, 0x8E);
    SetIDTEntry(14, isr14, 0x8E);
    SetIDTEntry(15, isr15, 0x8E);
    SetIDTEntry(16, isr16, 0x8E);
    SetIDTEntry(17, isr17, 0x8E);
    SetIDTEntry(18, isr18, 0x8E);
    SetIDTEntry(19, isr19, 0x8E);
    SetIDTEntry(20, isr20, 0x8E);
    SetIDTEntry(21, isr21, 0x8E);
    SetIDTEntry(22, isr22, 0x8E);
    SetIDTEntry(23, isr23, 0x8E);
    SetIDTEntry(24, isr24, 0x8E);
    SetIDTEntry(25, isr25, 0x8E);
    SetIDTEntry(26, isr26, 0x8E);
    SetIDTEntry(27, isr27, 0x8E);
    SetIDTEntry(28, isr28, 0x8E);
    SetIDTEntry(29, isr29, 0x8E);
    SetIDTEntry(30, isr30, 0x8E);
    SetIDTEntry(31, isr31, 0x8E);

    /* 하드웨어 인터럽트 핸들러 등록 (벡터 32번부터) */
    SetIDTEntry(32, irq32, 0x8E); // IRQ 0: Timer
    SetIDTEntry(33, irq33, 0x8E); // IRQ 1: Keyboard
    SetIDTEntry(44, irq44, 0x8E); // IRQ 12: Mouse
    SetIDTEntry(48, irq48, 0x8E); // APIC Timer

    /* IDT Pointer 설정 */
    idt_ptr.limit = (sizeof(struct IDTEntry) * 256) - 1;
    idt_ptr.base  = (uint64_t)&idt;

    /* IDT 로드 (어셈블리 함수 호출) */
    LoadIDT(&idt_ptr);
}
