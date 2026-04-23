#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>
#include <stddef.h>

#ifndef NULL
#define NULL ((void*)0)
#endif

/*
 * BootInfo 구조체: UEFI 부트로더에서 커널로 전달되는 시스템 정보입니다.
 */
typedef struct {
    uint32_t *framebuffer;           // 프레임버퍼(VRAM)의 시작 물리 주소
    uint64_t screen_size;            // 전체 화면 픽셀 수 (가로 * 세로)
    uint32_t horizontal_resolution;  // 가로 해상도 (픽셀 단위)
    uint32_t vertical_resolution;    // 세로 해상도 (픽셀 단위)
    void *mmap;                      // UEFI 메모리 맵 데이터 버퍼 주소
    uint64_t mmap_size;              // 메모리 맵 전체 버퍼 크기
    uint64_t descriptor_size;        // 각 메모리 기술자(Descriptor)의 크기
} BootInfo;

/*
 * EFI_MEMORY_DESCRIPTOR: UEFI에서 정의한 메모리 영역 정보 구조체입니다.
 * 커널이 메모리 맵을 파싱할 때 각 엔트리의 구조를 알기 위해 사용합니다.
 */
typedef struct {
    uint32_t type;                   // 메모리 타입 (가용, 예약, 펌웨어 등)
    uint32_t pad;
    uint64_t physical_start;         // 해당 영역의 시작 물리 주소
    uint64_t virtual_start;          // 가상 주소 (보통 0)
    uint64_t number_of_pages;        // 페이지(4KB) 단위 크기
    uint64_t attribute;              // 메모리 속성 (RW/Cache 등)
} MemoryDescriptor;

/* kmain: 커널의 진입점(Entry Point). */
void kmain(BootInfo *boot_info);

/* --- 그래픽 콘솔 기능 --- */

/* PutChar: 특정 위치(x, y)에 한 문자를 출력합니다. 배경색을 포함하여 잔상을 제거합니다. */
void PutChar(BootInfo *binfo, int x, int y, char c, uint32_t color, uint32_t bg_color);

/* PrintString: 현재 커서 위치부터 문자열을 출력합니다. */
void PrintString(BootInfo *binfo, const char *str, uint32_t color);

/* ClearScreen: 화면을 특정 색상으로 초기화합니다. */
void ClearScreen(BootInfo *binfo, uint32_t color);

/* Printf: 가변 인자 없이 문자열만 출력하는 임시 함수 */
void Printf(const char *str);

/* --- 인터럽트 제어 --- */
void EnableInterrupts(void);
void DisableInterrupts(void);
uint64_t GetRFLAGS(void);

// RFLAGS의 IF(Interrupt Flag) 비트는 9번 비트 (0x200)
#define RFLAGS_IF 0x200

static inline uint64_t SaveAndDisableInterrupts(void) {
    uint64_t rflags = GetRFLAGS();
    DisableInterrupts();
    return rflags;
}

static inline void RestoreInterrupts(uint64_t rflags) {
    if (rflags & RFLAGS_IF) {
        EnableInterrupts();
    }
}

#endif
