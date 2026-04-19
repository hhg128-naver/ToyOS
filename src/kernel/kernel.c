#include <stdio.h>
#include <stdlib.h>
#include "kernel.h"
#include "font.h"
#include "gdt.h"
#include "idt.h"
#include "pmm.h"
#include "vmm.h"
#include "heap.h"
#include "pic.h"
#include "task.h"

/* 어셈블리(asm_utils.asm)에서 정의됨 */
extern void EnableInterrupts();

/* 
 * 그래픽 콘솔을 위한 전역 상태 변수 
 */
static int cursor_x = 0;
static int cursor_y = 0;
BootInfo *boot_info_global; 
static uint32_t current_bg_color = 0x00000033; // 현재 시스템 배경색

/*
 * Printf: 가변 인자 없이 문자열만 출력하는 임시 함수
 * (이제 newlib의 printf가 내부적으로 syscalls.c의 _write를 호출하므로,
 *  기존 Printf 호출부도 점진적으로 교체 가능합니다.)
 */
void Printf(const char *str) {
    PrintString(boot_info_global, str, 0x00FFFFFF);
}

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
    current_bg_color = color;
}

/*
 * PutChar: 8x16 비트맵 폰트를 사용하여 한 문자를 프레임버퍼에 그립니다.
 * @param binfo: 부트 정보 (프레임버퍼 주소 및 해상도 포함)
 * @param x, y: 문자가 그려질 좌상단 시작 좌표
 * @param c: 출력할 ASCII 문자
 * @param color: 문자의 색상 (RGB 형식)
 * @param bg_color: 글자 비트가 0인 곳에 칠할 색상 (기존 잔상 제거용)
 */
void PutChar(BootInfo *binfo, int x, int y, char c, uint32_t color, uint32_t bg_color) {   
    /* 
     * EnglishFont는 1차원 배열이므로, ASCII 코드값(c)에 
     * 한 문자당 크기(16바이트)를 곱하여 해당 문자의 데이터 시작 위치를 찾습니다.
     */
    unsigned char *font_ptr = &EnglishFont[(unsigned char)c * 16];
    uint32_t *vidptr = binfo->framebuffer;

    for (int dy = 0; dy < 16; dy++) {
        for (int dx = 0; dx < 8; dx++) {
            if ((font_ptr[dy] << dx) & 0x80) {
                vidptr[(y + dy) * binfo->horizontal_resolution + (x + dx)] = color;
            } else {
                vidptr[(y + dy) * binfo->horizontal_resolution + (x + dx)] = bg_color;
            }
        }
    }
}

/*
 * PrintString: 문자열을 현재 커서 위치에 출력합니다.
 */
void PrintString(BootInfo *binfo, const char *str, uint32_t color) {
    for (int i = 0; str[i] != '\0'; i++) {
        /* 개행 문자 처리 */
        if (str[i] == '\n') {
            cursor_x = 0;
            cursor_y += 16;
        } else {
            PutChar(binfo, cursor_x, cursor_y, str[i], color, current_bg_color);
            cursor_x += 8;
        }

        /* 가로 화면 끝에 도달하면 자동 개행 */
        if (cursor_x + 8 > (int)binfo->horizontal_resolution) {
            cursor_x = 0;
            cursor_y += 16;
        }

        /* 세로 화면 끝에 도달하면 다시 상단으로 */
        if (cursor_y + 16 > (int)binfo->vertical_resolution) {
            cursor_y = 0;
        }
    }
}

/*
 * TaskB: 멀티태스킹 테스트를 위한 두 번째 태스크
 */
void TaskB() {
    uint64_t count = 0;
    while(1) {
        // 화면 오른쪽 하단에 노란색 'B' 출력
        PutChar(boot_info_global, boot_info_global->horizontal_resolution - 16, 
                boot_info_global->vertical_resolution - 32, 'B', 0x00FFFF00, current_bg_color);
        
        // 실행 중임을 증명하는 카운터 출력 (배경색을 함께 그려서 잔상 제거)
        if (count % 1000000 == 0) {
            char val = (count / 1000000 % 10) + '0';
            PutChar(boot_info_global, boot_info_global->horizontal_resolution - 32, 
                    boot_info_global->vertical_resolution - 32, val, 0x00FFFF00, current_bg_color);
        }
        count++;
    }
}

/*
 * kmain: 커널 메인 로직
 */
void kmain(BootInfo *boot_info)
{
    boot_info_global = boot_info;

    /* 1. 화면 초기화 (진한 파란색 배경) */
    ClearScreen(boot_info, 0x00000033);

    /* 2. 프로세서 환경 설정 (GDT/IDT) */
    Printf("Initializing System Tables (GDT/IDT)...\n");
    InitGDT();
    InitIDT();

    /* 3. 메모리 관리자 초기화 (PMM/VMM) */
    PMM_Init(boot_info);
    VMM_Init(boot_info);

    /* 4. 커널 힙 초기화 (기존 ToyOS 전용 힙) */
    Heap_Init(boot_info);

    /* 5. 환영 메시지 및 Newlib 테스트 */
    Printf("\nWelcome to ToyOS! (UEFI 64-bit Mode)\n");
    Printf("------------------------------------\n");
    
    // Newlib printf 테스트
    printf("Newlib printf Test: Success! [Value: %d, Hex: 0x%x]\n", 2026, 0xABCDE);

    // Newlib malloc 테스트
    void* nptr = malloc(1024);
    if (nptr) {
        printf("Newlib malloc Test: OK (Address: %p)\n", nptr);
        free(nptr);
    } else {
        printf("Newlib malloc Test: FAILED\n");
    }

    /* 6. 인터럽트 및 PIC 초기화 */
    PIC_Init();
    
    /* 7. 멀티태스킹 초기화 */
    InitializeTaskSystem();
    CreateTask(TaskB);

    EnableInterrupts();
    Printf("System Ready with Multitasking.\n");

    Printf("\nToyOS is now running with Newlib support.\n");
    Printf("Entering Task A loop...\n");

    uint64_t countA = 0;
    while(1) {
        PutChar(boot_info_global, 10, boot_info_global->vertical_resolution - 32, 'A', 0x00FF00FF, current_bg_color);
        
        if (countA % 1000000 == 0) {
            char val = (countA / 1000000 % 10) + '0';
            PutChar(boot_info_global, 30, boot_info_global->vertical_resolution - 32, val, 0x00FF00FF, current_bg_color);
        }
        countA++;
    }
}
