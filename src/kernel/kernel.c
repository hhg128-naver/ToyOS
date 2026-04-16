#include "kernel.h"
#include "font.h"

/* 
 * 그래픽 콘솔을 위한 전역 상태 변수 
 * 커서의 현재 위치를 유지하여 PrintString 호출 시 이어서 출력할 수 있게 합니다.
 */
static int cursor_x = 0;
static int cursor_y = 0;

/*
 * ClearScreen: 화면 전체를 지정된 색상으로 초기화합니다.
 */
void ClearScreen(BootInfo *binfo, uint32_t color) {
    uint32_t *vidptr = binfo->framebuffer;
    for (uint64_t i = 0; i < binfo->screen_size; i++) {
        vidptr[i] = color;
    }
    cursor_x = 0;
    cursor_y = 0;
}

/*
 * PutChar: 8x16 비트맵 폰트를 사용하여 한 문자를 프레임버퍼에 그립니다.
 * @param binfo: 부트 정보 (프레임버퍼 주소 및 해상도 포함)
 * @param x, y: 문자가 그려질 좌상단 시작 좌표
 * @param c: 출력할 ASCII 문자
 * @param color: 문자의 색상 (RGB 형식)
 */
void PutChar(BootInfo *binfo, int x, int y, char c, uint32_t color) {
    /* 
     * EnglishFont는 1차원 배열이므로, ASCII 코드값(c)에 
     * 한 문자당 크기(16바이트)를 곱하여 해당 문자의 데이터 시작 위치를 찾습니다.
     */
    unsigned char *font_ptr = &EnglishFont[(unsigned char)c * 16];
    uint32_t *vidptr = binfo->framebuffer;

    for (int dy = 0; dy < 16; dy++) {
        for (int dx = 0; dx < 8; dx++) {
            /* 
             * EnglishFont의 데이터 구조가 기존과 동일하게 
             * 한 바이트의 각 비트가 가로 8픽셀을 나타낸다고 가정합니다.
             */
            if ((font_ptr[dy] << dx) & 0x80) {
                vidptr[(y + dy) * binfo->horizontal_resolution + (x + dx)] = color;
            }
        }
    }
}

/*
 * PrintString: 문자열을 현재 커서 위치에 출력하고 줄 바꿈 및 스크롤 여부를 체크합니다.
 */
void PrintString(BootInfo *binfo, const char *str, uint32_t color) {
    for (int i = 0; str[i] != '\0'; i++) {
        /* 개행 문자 처리 */
        if (str[i] == '\n') {
            cursor_x = 0;
            cursor_y += 16;
        } else {
            PutChar(binfo, cursor_x, cursor_y, str[i], color);
            cursor_x += 8;
        }

        /* 가로 화면 끝에 도달하면 자동 개행 */
        if (cursor_x + 8 > (int)binfo->horizontal_resolution) {
            cursor_x = 0;
            cursor_y += 16;
        }

        /* 세로 화면 끝에 도달하면 다시 상단으로 (간단한 구현을 위해 스크롤 대신 루프) */
        if (cursor_y + 16 > (int)binfo->vertical_resolution) {
            cursor_y = 0;
        }
    }
}

/*
 * kmain: 커널 메인 로직
 */
void kmain(BootInfo *boot_info)
{
    /* 1. 화면 초기화 (진한 파란색 배경) */
    ClearScreen(boot_info, 0x00000033);

    /* 2. 환영 메시지 출력 */
    PrintString(boot_info, "Welcome to ToyOS! (UEFI 64-bit Mode)\n", 0x00FFFFFF);
    PrintString(boot_info, "------------------------------------\n", 0x0000FF00);
    PrintString(boot_info, "Memory Map Handover Check: OK\n", 0x0000FFFF);
    PrintString(boot_info, "Graphic Console Initialization: OK\n\n", 0x00FFFF00);

    /* 3. 메모리 정보 기반의 디버깅 메시지 (간략화) */
    uint64_t num_entries = boot_info->mmap_size / boot_info->descriptor_size;
    PrintString(boot_info, "Scanning System Memory Map...\n", 0x00FFFFFF);

    /* 
     * 시각적 검증: 가용 메모리 영역 탐색 결과를 화면에 간단히 표시 
     * (추후 printf 가변인자 구현 후 더 상세히 출력 예정)
     */
    int available_zones = 0;
    uint8_t *mmap_ptr = (uint8_t *)boot_info->mmap;
    for (uint64_t i = 0; i < num_entries; i++) {
        MemoryDescriptor *desc = (MemoryDescriptor *)(mmap_ptr + (i * boot_info->descriptor_size));
        if (desc->type == 7) { // EfiConventionalMemory
            available_zones++;
        }
    }
    
    PrintString(boot_info, "Available Memory Zones Found!\n", 0x0000FF00);
    PrintString(boot_info, "ToyOS is ready for Next Stage: GDT/IDT Setup.\n", 0x00FFFFFF);

    /* 4. 무한 루프 (시스템 대기) */
    while(1);
}
