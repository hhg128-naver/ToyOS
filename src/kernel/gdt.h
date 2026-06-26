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
#define GDT_TSS_START    0x28 // Index 5 (16 bytes, TSS Start selector)

/* ============================================================
 * GDT Access Byte 비트 필드 매크로 (비트 시프트 형식)
 * ============================================================
 *  비트 7    : P   (Present)          - 세그먼트 존재 여부
 *  비트 6-5  : DPL (Descriptor Privilege Level) - 특권 레벨 (0~3)
 *  비트 4    : S   (Descriptor Type)  - 1=코드/데이터, 0=시스템(TSS/LDT)
 *  비트 3    : E   (Executable)       - 1=코드, 0=데이터
 *  비트 2    : DC  (Direction/Conforming)
 *  비트 1    : RW  (Read/Write)       - 코드:읽기허용, 데이터:쓰기허용
 *  비트 0    : A   (Accessed)         - CPU가 자동 설정
 * ============================================================ */
#define GDT_ACCESS_ACCESSED      (1 << 0)  /* 0x01: Accessed */
#define GDT_ACCESS_READABLE      (1 << 1)  /* 0x02: Readable (코드용) */
#define GDT_ACCESS_WRITABLE      (1 << 1)  /* 0x02: Writable (데이터용) */
#define GDT_ACCESS_EXECUTABLE    (1 << 3)  /* 0x08: Executable */
#define GDT_ACCESS_CODE_DATA     (1 << 4)  /* 0x10: Descriptor Type */
#define GDT_ACCESS_DPL0          (0 << 5)  /* 0x00: Ring 0 (커널) */
#define GDT_ACCESS_DPL3          (3 << 5)  /* 0x60: Ring 3 (유저) */
#define GDT_ACCESS_PRESENT       (1 << 7)  /* 0x80: Present */
#define GDT_ACCESS_TSS_AVAILABLE 0x09      /* TSS (Available, 64-bit) */

/* 자주 쓰는 Access 바이트 조합 (편의 매크로) */
#define GDT_ACCESS_KERNEL_CODE  (GDT_ACCESS_PRESENT | GDT_ACCESS_DPL0 | GDT_ACCESS_CODE_DATA | GDT_ACCESS_EXECUTABLE | GDT_ACCESS_READABLE) /* = 0x9A */
#define GDT_ACCESS_KERNEL_DATA  (GDT_ACCESS_PRESENT | GDT_ACCESS_DPL0 | GDT_ACCESS_CODE_DATA | GDT_ACCESS_WRITABLE) /* = 0x92 */
#define GDT_ACCESS_USER_CODE    (GDT_ACCESS_PRESENT | GDT_ACCESS_DPL3 | GDT_ACCESS_CODE_DATA | GDT_ACCESS_EXECUTABLE | GDT_ACCESS_READABLE | GDT_ACCESS_ACCESSED)   /* = 0xFB */
#define GDT_ACCESS_USER_DATA    (GDT_ACCESS_PRESENT | GDT_ACCESS_DPL3 | GDT_ACCESS_CODE_DATA | GDT_ACCESS_WRITABLE | GDT_ACCESS_ACCESSED)   /* = 0xF3 */
#define GDT_ACCESS_TSS          (GDT_ACCESS_PRESENT | GDT_ACCESS_TSS_AVAILABLE) /* = 0x89 */

/* ============================================================
 * GDT Flags (Granularity 바이트 상위 4비트) 매크로
 * ============================================================
 *  비트 7 : G   (Granularity)  - 1=Limit 단위 4KB, 0=1바이트
 *  비트 6 : DB  (Default Size) - 1=32비트, 0=16비트 (Long Mode에서 반드시 0)
 *  비트 5 : L   (Long Mode)    - 1=64비트 코드 세그먼트
 *  비트 4 : AVL (Available)    - OS 자유 사용
 * ============================================================ */
#define GDT_FLAG_LONG_MODE       (1 << 5)  /* 0x20: L-비트 */
#define GDT_FLAG_SIZE_32         (1 << 6)  /* 0x40: DB-비트 */
#define GDT_FLAG_GRANULARITY_4K  (1 << 7)  /* 0x80: G-비트 */

/* GDT 초기화 함수 */
void InitGDT();
void SetTSSStack(uint8_t cpu_index, uint64_t stack_addr);

/**
 * InitGDT_AP: AP에 BSP의 GDT를 로드합니다.
 * AP가 ap_entry()에서 호출하여 커널 세그먼트를 활성화합니다.
 */
void InitGDT_AP(uint8_t cpu_index);

/* BSP의 GDT 포인터 (AP에서 접근) */
extern struct GDTPtr gdt_ptr;

#ifdef __cplusplus
extern "C" {
#endif
/* 어셈블리에서 정의할 함수들 */
extern void sLoadGDT(struct GDTPtr *gdt_ptr);
extern void LoadTSS(uint16_t tss_selector);
#ifdef __cplusplus
}
#endif

#endif
