#ifndef KERNEL_H
#define KERNEL_H

/*
 * 표준 정수형 정의: 베어메탈 환경에서는 라이브러리가 없으므로 직접 정의하여
 * 데이터 크기를 명확히 관리합니다.
 */
typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;

/*
 * BootInfo 구조체: UEFI 부트로더에서 커널로 전달되는 시스템 정보입니다.
 * 이 구조체는 부트로더의 정의와 바이너리 수준에서 일치해야 합니다.
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

/* PutChar: 특정 위치(x, y)에 한 문자를 출력합니다. */
void PutChar(BootInfo *binfo, int x, int y, char c, uint32_t color);

/* PrintString: 현재 커서 위치부터 문자열을 출력합니다. */
void PrintString(BootInfo *binfo, const char *str, uint32_t color);

/* ClearScreen: 화면을 특정 색상으로 초기화합니다. */
void ClearScreen(BootInfo *binfo, uint32_t color);

#endif
