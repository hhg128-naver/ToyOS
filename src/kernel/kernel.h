#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>
#include <stddef.h>

#ifndef NULL
#define NULL ((void *)0)
#endif

/*
 * BootInfo 구조체: UEFI 부트로더에서 커널로 전달되는 시스템 정보입니다.
 */
typedef struct
{
    uint32_t *framebuffer;          // 프레임버퍼(VRAM)의 시작 물리 주소
    uint64_t screen_size;           // 전체 화면 픽셀 수 (가로 * 세로)
    uint32_t horizontal_resolution; // 가로 해상도 (픽셀 단위)
    uint32_t vertical_resolution;   // 세로 해상도 (픽셀 단위)
    void *mmap;                     // UEFI 메모리 맵 데이터 버퍼 주소
    uint64_t mmap_size;             // 메모리 맵 전체 버퍼 크기
    uint64_t descriptor_size;       // 각 메모리 기술자(Descriptor)의 크기
    void *rsdp;                     // ACPI RSDP(Root System Description Pointer) 주소
} BootInfo;

/*
 * EFI_MEMORY_DESCRIPTOR: UEFI에서 정의한 메모리 영역 정보 구조체입니다.
 * 커널이 메모리 맵을 파싱할 때 각 엔트리의 구조를 알기 위해 사용합니다.
 */
typedef struct
{
    uint32_t type; // 메모리 타입 (가용, 예약, 펌웨어 등)
    uint32_t pad;
    uint64_t physical_start;  // 해당 영역의 시작 물리 주소
    uint64_t virtual_start;   // 가상 주소 (보통 0)
    uint64_t number_of_pages; // 페이지(4KB) 단위 크기
    uint64_t attribute;       // 메모리 속성 (RW/Cache 등)
} MemoryDescriptor;

/* kmain: 커널의 진입점(Entry Point). */
void kmain(BootInfo *boot_info);

#include "console.h"

/* --- 인터럽트 제어 --- */
void EnableInterrupts(void);
void DisableInterrupts(void);
uint64_t GetRFLAGS(void);

/* --- 제어 레지스터 및 FPU --- */
uint64_t ReadCR0(void);
void WriteCR0(uint64_t cr0);
uint64_t ReadCR4(void);
void WriteCR4(uint64_t cr4);
void InitFPU(void);

/* --- I/O 포트 제어 (asm_utils.asm) --- */
void outb(uint16_t port, uint8_t data);
uint8_t inb(uint16_t port);
void outw(uint16_t port, uint16_t data);
uint16_t inw(uint16_t port);
void insw(uint16_t port, void *buffer, uint32_t count);

// RFLAGS의 IF(Interrupt Flag) 비트는 9번 비트 (0x200)
#define RFLAGS_IF 0x200

static inline uint64_t SaveAndDisableInterrupts(void)
{
    uint64_t rflags = GetRFLAGS();
    DisableInterrupts();
    return rflags;
}

static inline void RestoreInterrupts(uint64_t rflags)
{
    if (rflags & RFLAGS_IF)
    {
        EnableInterrupts();
    }
}

#endif
