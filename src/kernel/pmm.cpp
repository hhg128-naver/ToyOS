#include "pmm.h"

static uint8_t *bitmap;
static uint64_t total_pages;
static uint64_t free_pages;
static uint64_t bitmap_size;

/* Helper functions to manage the bitmap */
static void Bitmap_Set(uint64_t page_idx)
{
	bitmap[page_idx / 8] |= (1 << (page_idx % 8));
}

static void Bitmap_Clear(uint64_t page_idx)
{
	bitmap[page_idx / 8] &= ~(1 << (page_idx % 8));
}

static int Bitmap_Get(uint64_t page_idx)
{
	return (bitmap[page_idx / 8] >> (page_idx % 8)) & 1;
}

void PMM_Init(BootInfo *binfo)
{
	uint64_t max_addr = 0;
	uint64_t num_entries = binfo->mmap_size / binfo->descriptor_size;
	uint8_t *mmap_ptr = (uint8_t *)binfo->mmap;

	/* 1. Calculate total memory size from UEFI memory map */
	for (uint64_t i = 0; i < num_entries; i++)
	{
		MemoryDescriptor *desc = (MemoryDescriptor *)(mmap_ptr + (i * binfo->descriptor_size));
		uint64_t end = desc->physical_start + (desc->number_of_pages * PAGE_SIZE);
		if (end > max_addr)
		{
			max_addr = end;
		}
	}

	total_pages = max_addr / PAGE_SIZE;
	bitmap_size = total_pages / 8;
	if (total_pages % 8 != 0)
		bitmap_size++;

	/* 2. 비트맵을 저장할 적절한 위치 선정 (단순화를 위해 2MB 주소 근처 사용 가능 영역 탐색) */
	/* 실제로는 메모리 맵에서 'LoaderData'나 'ConventionalMemory' 중 충분히 큰 곳을 찾아야 함 */
	bitmap = (uint8_t *)0x200000; // 임시: 2MB 위치 (충분한 공간이 있다고 가정)

	/* 3. Mark EfiConventionalMemory (Type 7) as free */
	for (uint64_t i = 0; i < bitmap_size; i++)
	{
		bitmap[i] = 0xFF;
	}

	free_pages = 0;
	for (uint64_t i = 0; i < num_entries; i++)
	{
		MemoryDescriptor *desc = (MemoryDescriptor *)(mmap_ptr + (i * binfo->descriptor_size));
		if (desc->type == 7)
		{ // EfiConventionalMemory (사용 가능)
			for (uint64_t p = 0; p < desc->number_of_pages; p++)
			{
				uint64_t page_idx = (desc->physical_start / PAGE_SIZE) + p;
				Bitmap_Clear(page_idx);
				free_pages++;
			}
		}
	}

	/* 4. Mandatory Protection: Mark 0MB ~ 4MB as used
	 * This covers: Null page, BIOS area, Kernel, Stack, and the Bitmap itself.
	 */
	for (uint64_t i = 0; i < (0x4000000 / PAGE_SIZE); i++)
	{
		if (Bitmap_Get(i) == 0)
		{
			Bitmap_Set(i);
			free_pages--;
		}
	}
}

void *PMM_AllocPage()
{
	for (uint64_t i = 0; i < total_pages; i++)
	{
		if (Bitmap_Get(i) == 0)
		{
			Bitmap_Set(i);
			free_pages--;
			return (void *)(i * PAGE_SIZE);
		}
	}
	return NULL;
}

void PMM_FreePage(void *addr)
{
	uint64_t page_idx = (uint64_t)addr / PAGE_SIZE;
	Bitmap_Clear(page_idx);
	free_pages++;
}

uint64_t PMM_GetTotalMemory() { return total_pages * PAGE_SIZE; }
uint64_t PMM_GetFreeMemory() { return free_pages * PAGE_SIZE; }
