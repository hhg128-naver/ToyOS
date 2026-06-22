#ifndef PMM_H
#define PMM_H

#include "kernel.h"

#define PAGE_SIZE 4096

#ifdef __cplusplus
extern "C" {
#endif

/* PMM 초기화 함수 */
void PMM_Init(BootInfo *binfo);

/* 물리 페이지 할당/해제 */
void* PMM_AllocPage();
void PMM_FreePage(void* addr);

/* 시스템 총 메모리 정보 */
uint64_t PMM_GetTotalMemory();
uint64_t PMM_GetFreeMemory();

#ifdef __cplusplus
}
#endif

#endif
