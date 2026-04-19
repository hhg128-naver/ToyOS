#ifndef VMM_H
#define VMM_H

#include "kernel.h"

/* 페이지 테이블 엔트리 플래그 */
#define PAGE_PRESENT  (1ULL << 0)
#define PAGE_WRITABLE (1ULL << 1)
#define PAGE_USER     (1ULL << 2)

/* 페이지 테이블 엔트리 구조 */
typedef uint64_t pt_entry;

/* VMM 초기화 및 매핑 */
void VMM_Init(BootInfo *binfo);
void VMM_MapPage(void* virtual_addr, void* physical_addr, uint64_t flags);

/* 페이지 디렉토리 로드 */
extern void LoadPageTable(void* pml4_addr);

#endif
