#ifndef HEAP_H
#define HEAP_H

#include <stdint.h>
#include <stddef.h>
#include "vmm.h"
#include "pmm.h"

/* 힙 영역 설정: 16MB 지점부터 2MB 확보 (테스트용) */
#define HEAP_START 0x01000000
#define HEAP_SIZE  (2 * 1024 * 1024) 

/* 힙 블록 헤더 구조체 */
typedef struct HeapBlock {
    uint64_t size;           // 블록 크기 (헤더 제외)
    struct HeapBlock* next;  // 다음 블록 포인터
    int is_free;             // 1: 가용, 0: 사용 중
} HeapBlock;

/* 힙 관리 함수 */
void Heap_Init(BootInfo *binfo);
void* kmalloc(size_t size);
void kfree(void* ptr);

#endif
