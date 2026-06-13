#include "gdt.h"
#include "smp.h"
#include <string.h>

/* GDT 엔트리 배열 (최소한의 구성 -> 확장) 
 * 0: NULL, 1: KCode, 2: KData, 3: UData, 4: UCode, 5-6: TSS for BSP, 7+: TSS for APs
 * 16비트 TSSDescriptor는 2개의 GDTEntry 슬롯을 차지하므로, GDT 크기는:
 * 5 (NULL ~ UserCode) + SMP_MAX_CPUS * 2
 */
static struct GDTEntry gdt[5 + SMP_MAX_CPUS * 2];
struct GDTPtr gdt_ptr;   /* AP에서 접근할 수 있도록 non-static */
static struct TSS tss_array[SMP_MAX_CPUS];

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

void SetTSSStack(uint8_t cpu_index, uint64_t stack_addr) 
{
    if (cpu_index < SMP_MAX_CPUS) 
    {
        tss_array[cpu_index].rsp0 = stack_addr;
    }
}

void InitGDT() {
    /* NULL Descriptor */
    SetGDTEntry(0, 0, 0, 0, 0);

    /* Kernel Code/Data Segment (64-bit) - Index 1(0x08)/2(0x10)*/
    SetGDTEntry(1, 0, 0xFFFFFFFF, 0x9A, 0xA0);
    SetGDTEntry(2, 0, 0xFFFFFFFF, 0x92, 0xC0);

    /* User Data Segment (64-bit) - Index 3 (0x18 | 3 = 0x1B)
     * Access: 0xF3 (Present, Ring 3, Data, Writable, Accessed)
     */
    SetGDTEntry(3, 0, 0xFFFFFFFF, 0xF3, 0xC0);

    /* User Code Segment (64-bit) - Index 4 (0x20 | 3 = 0x23)
     * Access: 0xFB (Present, Ring 3, Code, Readable, Accessed)
     */
    SetGDTEntry(4, 0, 0xFFFFFFFF, 0xFB, 0xA0);

    /* TSS Descriptor - Index 5 (0x28) (CPU 0 / BSP 전용) */
    memset(&tss_array[0], 0, sizeof(struct TSS));
    tss_array[0].iopb_offset = sizeof(struct TSS);
    SetTSSEntry(5, (uint64_t)&tss_array[0], sizeof(struct TSS) - 1, 0x89);

    /* GDT Pointer 설정 */
    gdt_ptr.limit = (sizeof(struct GDTEntry) * (5 + SMP_MAX_CPUS * 2)) - 1;
    gdt_ptr.base  = (uint64_t)&gdt;

    /* GDT 로드 */
    sLoadGDT(&gdt_ptr);

    /* TSS 로드 */
    LoadTSS(0x28);
}

/*
 * InitGDT_AP: AP(Application Processor)에 GDT를 로드하고 각 CPU 전용 TSS를 설정합니다.
 * sLoadGDT()는 lgdt 및 far return으로 CS를 0x08로 갱신하고
 * DS/ES/FS/GS/SS를 0x10으로 설정합니다.
 * AP도 각자의 커널 스택(RSP0)을 관리해야 하므로 TSS를 로드합니다.
 */
void InitGDT_AP(uint8_t cpu_index)
{
    sLoadGDT(&gdt_ptr);

    if (cpu_index < SMP_MAX_CPUS) 
    {
        /* 각 CPU에 맞는 GDT TSSDescriptor 설정 (Index 5 + cpu_index * 2) */
        int tss_gdt_index = 5 + (int)cpu_index * 2; 
        memset(&tss_array[cpu_index], 0, sizeof(struct TSS));
        tss_array[cpu_index].iopb_offset = sizeof(struct TSS);
        SetTSSEntry(tss_gdt_index, (uint64_t)&tss_array[cpu_index], sizeof(struct TSS) - 1, 0x89);

        /* TSS 로드 (GDT selector = tss_gdt_index * 8) */
        LoadTSS(tss_gdt_index * 8);
    }
}
