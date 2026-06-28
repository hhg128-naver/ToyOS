#ifndef VMM_H
#define VMM_H

#include "kernel.h"

/* 페이지 테이블 엔트리 플래그 */
#define PAGE_PRESENT  (1ULL << 0)
#define PAGE_WRITABLE (1ULL << 1)
#define PAGE_USER          (1ULL << 2)
#define PAGE_WRITE_THROUGH (1ULL << 3)  /* PWT 비트 */
#define PAGE_CACHE_DISABLE (1ULL << 4)  /* PCD 비트 */

/* 페이지 테이블 엔트리 구조 */
typedef uint64_t pt_entry;

#ifdef __cplusplus
extern "C" {
#endif

/* VMM 초기화 및 매핑 */
void VMM_Init(BootInfo *binfo);
void VMM_MapPage(void* virtual_addr, void* physical_addr, uint64_t flags);
void VMM_MapPageEx(pt_entry* pml4, void* virtual_addr, void* physical_addr, uint64_t flags);

/* 새로운 주소 공간(PML4) 생성 및 해제 */
void* VMM_CreateAddressSpace();
void VMM_FreeAddressSpace(void* pml4);

/* 페이지 디렉토리 로드 */
extern void LoadPageTable(void* pml4_addr);
extern void* GetCR3();
extern void InvalidatePage(void* virtual_addr);

#ifdef __cplusplus
}
#endif

#endif
