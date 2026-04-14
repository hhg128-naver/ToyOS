#include "kernel.h"

// 부트로더로부터 전달받을 정보를 담은 구조체 정의
typedef struct {
    unsigned int *framebuffer;
    unsigned long screen_size;
    unsigned int horizontal_resolution;
    unsigned int vertical_resolution;
} BootInfo;

void kmain(BootInfo *boot_info)
{
    unsigned int *vidptr = boot_info->framebuffer;
    unsigned long screensize = boot_info->screen_size;

    // 화면을 파란색으로 초기화 (64비트 커널 테스트)
    for (unsigned long i = 0; i < screensize; i++) {
        vidptr[i] = 0x000000FF; // Blue
    }

    // 간단한 텍스트 출력을 위한 'X' 표시 (픽셀 단위 제어 예시)
    for (int i = 0; i < 100; i++) {
        vidptr[i + (boot_info->horizontal_resolution * i)] = 0x00FFFFFF; // White diagonal line
    }

    while(1); // 무한 루프
}
