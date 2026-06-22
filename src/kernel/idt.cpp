#include "idt.h"

static struct IDTEntry idt[256];
struct IDTPtr idt_ptr;   /* AP에서 접근할 수 있도록 non-static */

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
        SetIDTEntry(i, nullptr, 0);
    }

    /* 모든 예외 핸들러 등록 (0~31) */
    SetIDTEntry(0,  (void*)isr0,  0x8E);
    SetIDTEntry(1,  (void*)isr1,  0x8E);
    SetIDTEntry(2,  (void*)isr2,  0x8E);
    SetIDTEntry(3,  (void*)isr3,  0x8E);
    SetIDTEntry(4,  (void*)isr4,  0x8E);
    SetIDTEntry(5,  (void*)isr5,  0x8E);
    SetIDTEntry(6,  (void*)isr6,  0x8E);
    SetIDTEntry(7,  (void*)isr7,  0x8E);
    SetIDTEntry(8,  (void*)isr8,  0x8E);
    SetIDTEntry(9,  (void*)isr9,  0x8E);
    SetIDTEntry(10, (void*)isr10, 0x8E);
    SetIDTEntry(11, (void*)isr11, 0x8E);
    SetIDTEntry(12, (void*)isr12, 0x8E);
    SetIDTEntry(13, (void*)isr13, 0x8E);
    SetIDTEntry(14, (void*)isr14, 0x8E);
    SetIDTEntry(15, (void*)isr15, 0x8E);
    SetIDTEntry(16, (void*)isr16, 0x8E);
    SetIDTEntry(17, (void*)isr17, 0x8E);
    SetIDTEntry(18, (void*)isr18, 0x8E);
    SetIDTEntry(19, (void*)isr19, 0x8E);
    SetIDTEntry(20, (void*)isr20, 0x8E);
    SetIDTEntry(21, (void*)isr21, 0x8E);
    SetIDTEntry(22, (void*)isr22, 0x8E);
    SetIDTEntry(23, (void*)isr23, 0x8E);
    SetIDTEntry(24, (void*)isr24, 0x8E);
    SetIDTEntry(25, (void*)isr25, 0x8E);
    SetIDTEntry(26, (void*)isr26, 0x8E);
    SetIDTEntry(27, (void*)isr27, 0x8E);
    SetIDTEntry(28, (void*)isr28, 0x8E);
    SetIDTEntry(29, (void*)isr29, 0x8E);
    SetIDTEntry(30, (void*)isr30, 0x8E);
    SetIDTEntry(31, (void*)isr31, 0x8E);

    /* 하드웨어 인터럽트 핸들러 등록 (벡터 32번부터) */
    SetIDTEntry(32, (void*)irq32, 0x8E); // IRQ 0: Timer
    SetIDTEntry(33, (void*)irq33, 0x8E); // IRQ 1: Keyboard
    SetIDTEntry(44, (void*)irq44, 0x8E); // IRQ 12: Mouse
    SetIDTEntry(48, (void*)irq48, 0x8E); // APIC Timer
    SetIDTEntry(255, (void*)irq255, 0x8E); // LAPIC Spurious (벡터 0xFF, EOI 없이 즉시 반환)

    /* IDT Pointer 설정 */
    idt_ptr.limit = (sizeof(struct IDTEntry) * 256) - 1;
    idt_ptr.base  = (uint64_t)&idt;

    /* IDT 로드 (어셈블리 함수 호출) */
    LoadIDT(&idt_ptr);
}

/*
 * LoadIDT_AP: AP에 BSP의 IDT를 로드합니다.
 * 예외 핸들러 활성화를 위해 ap_entry()에서 호출됩니다.
 */
void LoadIDT_AP(void)
{
    LoadIDT(&idt_ptr);
}
