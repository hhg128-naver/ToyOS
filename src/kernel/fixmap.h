#ifndef FIXMAP_H
#define FIXMAP_H

#include "kernel.h"

/*
 * Fixmap (고정 가상 주소 매핑) 시스템
 *
 * 커널 가상 주소 공간의 최상단(Canonical High-Half)에 고정된 가상 주소 슬롯을
 * 정의하고, MMIO 장치 등의 물리 주소를 이 슬롯에 동적으로 매핑하여 사용합니다.
 *
 * FIXMAP_TOP: Fixmap 영역의 최상단 가상 주소 (페이지 정렬)
 * 각 Fixmap 슬롯은 FIXMAP_TOP에서 아래 방향으로 성장합니다.
 *   slot N의 가상 주소 = FIXMAP_TOP - (N * PAGE_SIZE)
 *
 * 이 방식은 Linux 커널의 Fixmap 관례를 따릅니다.
 */
#define FIXMAP_TOP  0xFFFFFFFFFFFFF000ULL

/*
 * 고정 주소 인덱스 열거형
 * 새로운 MMIO 장치를 추가할 때 __end_of_fixed_addresses 위에 항목을 추가합니다.
 */
enum fixed_addresses {
    FIX_APIC_BASE = 0,   /* Local APIC  (물리: 0xFEE00000) */
    FIX_IOAPIC_BASE,     /* I/O APIC    (물리: 0xFEC00000) */
    /* 향후 확장: FIX_HPET_BASE, FIX_FRAMEBUFFER 등 */
    __end_of_fixed_addresses  /* 반드시 마지막에 위치 */
};

#ifdef __cplusplus
extern "C" {
#endif

/*
 * fix_to_virt: Fixmap 인덱스를 가상 주소로 변환합니다.
 * @param idx: fixed_addresses 열거형 값
 * @return: 해당 슬롯의 가상 주소
 */
static inline void* fix_to_virt(enum fixed_addresses idx)
{
    return (void*)(FIXMAP_TOP - ((uint64_t)idx * 0x1000));
}

/*
 * Fixmap_Set: 지정 인덱스의 Fixmap 슬롯에 물리 주소를 매핑합니다.
 * @param idx: fixed_addresses 열거형 값
 * @param phys_addr: 매핑할 물리 주소 (4KB 정렬)
 * @param flags: 페이지 플래그 (PAGE_PRESENT | PAGE_WRITABLE | PAGE_CACHE_DISABLE 등)
 */
void Fixmap_Set(enum fixed_addresses idx, uint64_t phys_addr, uint64_t flags);

/*
 * Fixmap_Clear: 지정 인덱스의 Fixmap 매핑을 해제합니다.
 * @param idx: fixed_addresses 열거형 값
 */
void Fixmap_Clear(enum fixed_addresses idx);

#ifdef __cplusplus
}
#endif

#endif
