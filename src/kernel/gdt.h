#ifndef GDT_H
#define GDT_H

#include "kernel.h"

/* GDT 엔트리 구조체 (64비트 호환) */
struct GDTEntry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

/* GDT Pointer 구조체 */
struct GDTPtr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

/* GDT 초기화 함수 */
void InitGDT();

/* 어셈블리에서 정의할 GDT 로드 함수 */
extern void LoadGDT(struct GDTPtr *gdt_ptr);

#endif
