#ifndef GDT_H
#define GDT_H

#include "kernel.h"

/* GDT 엔트리 구조체 (8바이트) */
struct GDTEntry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

/* TSS GDT 엔트리 구조체 (16바이트, x86_64 전용) */
struct TSSDescriptor {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
    uint32_t base_highest;
    uint32_t reserved;
} __attribute__((packed));

/* TSS 구조체 (x86_64) */
struct TSS {
    uint32_t reserved0;
    uint64_t rsp0;      // Ring 0 Stack Pointer
    uint64_t rsp1;      // Ring 1 Stack Pointer
    uint64_t rsp2;      // Ring 2 Stack Pointer
    uint64_t reserved1;
    uint64_t ist1;      // Interrupt Stack Table 1
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb_offset;
} __attribute__((packed));

/* GDT Pointer 구조체 */
struct GDTPtr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

/* GDT 세그먼트 인덱스 정의 */
#define GDT_KERNEL_CODE  0x08
#define GDT_KERNEL_DATA  0x10
#define GDT_USER_DATA    0x1B // Index 3 | RPL 3
#define GDT_USER_CODE    0x23 // Index 4 | RPL 3
#define GDT_TSS          0x28 // Index 5 (16 bytes)

/* GDT 초기화 함수 */
void InitGDT();
void SetTSSStack(uint64_t stack_addr);

/**
 * InitGDT_AP: AP에 BSP의 GDT를 로드합니다.
 * AP가 ap_entry()에서 호출하여 커널 세그먼트를 활성화합니다.
 */
void InitGDT_AP(void);

/* BSP의 GDT 포인터 (AP에서 접근) */
extern struct GDTPtr gdt_ptr;

/* 어셈블리에서 정의할 함수들 */
extern void sLoadGDT(struct GDTPtr *gdt_ptr);
extern void LoadTSS(uint16_t tss_selector);

#endif
