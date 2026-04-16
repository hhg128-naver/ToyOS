#include "gdt.h"

/* GDT 엔트리 배열 (최소한의 구성) */
static struct GDTEntry gdt[3];
static struct GDTPtr gdt_ptr;

/* GDT 엔트리 설정 함수 */
void SetGDTEntry(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[num].base_low    = (base & 0xFFFF);
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high   = (base >> 24) & 0xFF;

    gdt[num].limit_low   = (limit & 0xFFFF);
    gdt[num].granularity = (limit >> 16) & 0x0F;

    gdt[num].granularity |= gran & 0xF0;
    gdt[num].access      = access;
}

void InitGDT() {
    /* NULL Descriptor */
    SetGDTEntry(0, 0, 0, 0, 0);

    /* Kernel Code Segment (64-bit)
     * Access: 0x9A (Present, Ring 0, Code, Readable)
     * Granularity: 0xA0 (64-bit mode flag set)
     */
    SetGDTEntry(1, 0, 0xFFFFFFFF, 0x9A, 0xA0);

    /* Kernel Data Segment (64-bit)
     * Access: 0x92 (Present, Ring 0, Data, Writable)
     * Granularity: 0xC0 (32-bit/64-bit common flags)
     */
    SetGDTEntry(2, 0, 0xFFFFFFFF, 0x92, 0xC0);

    /* GDT Pointer 설정 */
    gdt_ptr.limit = (sizeof(struct GDTEntry) * 3) - 1;
    gdt_ptr.base  = (uint64_t)&gdt;

    /* GDT 로드 (어셈블리 함수 호출) */
    LoadGDT(&gdt_ptr);
}
