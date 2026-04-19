#include "gdt.h"

/* GDT 엔트리 배열 (최소한의 구성 -> 확장) 
 * 0: NULL, 1: KCode, 2: KData, 3: UCode, 4: UData, 5-6: TSS
 */
static struct GDTEntry gdt[7];
static struct GDTPtr gdt_ptr;
static struct TSS tss;

/* GDT 엔트리 설정 함수 (일반 8바이트) */
void SetGDTEntry(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[num].base_low    = (base & 0xFFFF);
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high   = (base >> 24) & 0xFF;

    gdt[num].limit_low   = (limit & 0xFFFF);
    gdt[num].granularity = (limit >> 16) & 0x0F;

    gdt[num].granularity |= gran & 0xF0;
    gdt[num].access      = access;
}

/* TSS 엔트리 설정 함수 (16바이트) */
void SetTSSEntry(int num, uint64_t base, uint32_t limit, uint8_t access) {
    struct TSSDescriptor *desc = (struct TSSDescriptor *)&gdt[num];

    desc->limit_low    = (limit & 0xFFFF);
    desc->base_low     = (base & 0xFFFF);
    desc->base_middle  = (base >> 16) & 0xFF;
    desc->access       = access;
    desc->granularity  = (limit >> 16) & 0x0F;
    desc->base_high    = (base >> 24) & 0xFF;
    desc->base_highest = (base >> 32) & 0xFFFFFFFF;
    desc->reserved     = 0;
}

void SetTSSStack(uint64_t stack_addr) {
    tss.rsp0 = stack_addr;
}

void InitGDT() {
    /* NULL Descriptor */
    SetGDTEntry(0, 0, 0, 0, 0);

    /* Kernel Code Segment (64-bit) - Index 1 (0x08)
     * Access: 0x9A (Present, Ring 0, Code, Readable)
     * Granularity: 0xA0 (64-bit mode flag set)
     */
    SetGDTEntry(1, 0, 0xFFFFFFFF, 0x9A, 0xA0);

    /* Kernel Data Segment (64-bit) - Index 2 (0x10)
     * Access: 0x92 (Present, Ring 0, Data, Writable)
     * Granularity: 0xC0 (32-bit/64-bit common flags)
     */
    SetGDTEntry(2, 0, 0xFFFFFFFF, 0x92, 0xC0);

    /* User Data Segment (64-bit) - Index 3 (0x18 | 3 = 0x1B)
     * Access: 0xF2 (Present, Ring 3, Data, Writable)
     */
    SetGDTEntry(3, 0, 0xFFFFFFFF, 0xF2, 0xC0);

    /* User Code Segment (64-bit) - Index 4 (0x20 | 3 = 0x23)
     * Access: 0xFA (Present, Ring 3, Code, Readable)
     */
    SetGDTEntry(4, 0, 0xFFFFFFFF, 0xFA, 0xA0);

    /* TSS Descriptor - Index 5 (0x28)
     * Access: 0x89 (Present, Executable, Accessed)
     */
    for (int i = 0; i < sizeof(struct TSS); i++) ((char*)&tss)[i] = 0;
    tss.iopb_offset = sizeof(struct TSS);
    SetTSSEntry(5, (uint64_t)&tss, sizeof(struct TSS) - 1, 0x89);

    /* GDT Pointer 설정 */
    gdt_ptr.limit = (sizeof(struct GDTEntry) * 7) - 1;
    gdt_ptr.base  = (uint64_t)&gdt;

    /* GDT 로드 (어셈블리 함수 호출) */
    LoadGDT(&gdt_ptr);

    /* TSS 로드 */
    LoadTSS(GDT_TSS);
}
