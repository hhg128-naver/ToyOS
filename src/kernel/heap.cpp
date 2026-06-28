#include "heap.h"

static HeapBlock* free_list_start = NULL;

void Heap_Init(BootInfo* binfo)
{
	/* 1. 힙 영역을 위한 물리 메모리 할당 및 가상 주소 매핑 */
	/* HEAP_SIZE(2MB)는 512개의 4KB 페이지가 필요함 */
	for (uint64_t addr = HEAP_START; addr < HEAP_START + HEAP_SIZE; addr += PAGE_SIZE) 
	{
		void* phys = PMM_AllocPage();
		if (phys == NULL) return; // 메모리 부족
		VMM_MapPage((void*)addr, phys, PAGE_PRESENT | PAGE_WRITABLE);
	}

	/* 2. 초기 가용 리스트 생성 (전체 힙을 하나의 블록으로) */
	free_list_start = (HeapBlock*)HEAP_START;
	free_list_start->size = HEAP_SIZE - sizeof(HeapBlock);
	free_list_start->next = NULL;
	free_list_start->is_free = 1;
}

void* kmalloc(size_t size)
{
	if (size == 0) return NULL;

	HeapBlock* current = free_list_start;
	while (current != NULL) {
		/* 충분한 크기의 가용 블록 발견 */
		if (current->is_free && current->size >= size) {

			/* 블록 분할(Splitting) 여부 결정: 남은 공간이 헤더 + 최소 16바이트 이상일 때 */
			if (current->size >= size + sizeof(HeapBlock) + 16) {
				HeapBlock* next_block = (HeapBlock*)((uint8_t*)current + sizeof(HeapBlock) + size);
				next_block->size = current->size - size - sizeof(HeapBlock);
				next_block->next = current->next;
				next_block->is_free = 1;

				current->size = size;
				current->next = next_block;
			}

			current->is_free = 0;
			return (void*)((uint8_t*)current + sizeof(HeapBlock));
		}
		current = current->next;
	}

	return NULL; // 적합한 블록 없음
}

void kfree(void* ptr)
{
	if (ptr == NULL) return;

	/* 포인터로부터 헤더 위치 계산 */
	HeapBlock* block = (HeapBlock*)((uint8_t*)ptr - sizeof(HeapBlock));
	block->is_free = 1;

	/* 인접한 가용 블록 병합 (Coalescing) */
	HeapBlock* current = free_list_start;
	while (current != NULL) {
		if (current->is_free && current->next != NULL && current->next->is_free) {
			current->size += sizeof(HeapBlock) + current->next->size;
			current->next = current->next->next;
			/* 병합 후 다시 현재 블록부터 병합 가능한지 확인하기 위해 continue 대신 루프 유지 */
		}
		else {
			current = current->next;
		}
	}
}
