#ifndef KERNEL_H
#define KERNEL_H

/*
 * 표준 정수형 정의: 64비트 환경에서의 데이터 크기를 명시적으로 관리하기 위해 사용합니다.
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
    uint64_t screen_size;            // 전체 화면의 픽셀 수
    uint32_t horizontal_resolution;  // 가로 해상도 (픽셀)
    uint32_t vertical_resolution;    // 세로 해상도 (픽셀)
    void *mmap;                      // UEFI 메모리 맵 데이터 버퍼 포인터
    uint64_t mmap_size;              // 메모리 맵 전체 버퍼의 크기 (바이트 단위)
    uint64_t descriptor_size;        // 각 메모리 기술자(Descriptor)의 크기
} BootInfo;

/*
 * EFI_MEMORY_DESCRIPTOR: UEFI에서 정의한 메모리 영역 정보 구조체입니다.
 * 커널이 메모리 맵을 파싱할 때 각 엔트리의 구조를 알기 위해 사용합니다.
 */
typedef struct {
    uint32_t type;                   // 메모리 영역의 타입 (가용, 예약, 런타임 등)
    uint32_t pad;
    uint64_t physical_start;         // 해당 영역의 시작 물리 주소
    uint64_t virtual_start;          // 가상 주소 (보통 0)
    uint64_t number_of_pages;        // 페이지(4KB) 단위의 크기
    uint64_t attribute;              // 메모리 속성 (Read/Write/Cache 등)
} MemoryDescriptor;

/* kmain: 커널의 C언어 진입점입니다. */
void kmain(BootInfo *boot_info);

#endif
