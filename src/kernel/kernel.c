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
#include "timer.h"
#include "task.h"
#include "syscall.h"
#include "shell.h"

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
 * ScrollUp: 화면의 내용을 한 행(16 픽셀)만큼 위로 올립니다.
 */
static void ScrollUp(BootInfo *binfo) {
    uint32_t *vidptr = binfo->framebuffer;
    uint32_t width = binfo->horizontal_resolution;
    uint32_t height = binfo->vertical_resolution;
    
    /* 1. 모든 픽셀을 16행 위로 복사 */
    uint64_t scroll_pixels = (uint64_t)(height - 16) * width;
    uint64_t offset_pixels = (uint64_t)16 * width;
    
    for (uint64_t i = 0; i < scroll_pixels; i++) {
        vidptr[i] = vidptr[i + offset_pixels];
    }
    
    /* 2. 마지막 16행을 배경색으로 지움 */
    uint64_t total_pixels = (uint64_t)height * width;
    for (uint64_t i = scroll_pixels; i < total_pixels; i++) {
        vidptr[i] = current_bg_color;
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
        } else if (str[i] == '\b') {
            /* 백스페이스 처리 */
            if (cursor_x >= 8) {
                cursor_x -= 8;
                /* 현재 배경색으로 공백을 그려서 이전 문자를 지움 */
                PutChar(binfo, cursor_x, cursor_y, ' ', current_bg_color, current_bg_color);
            }
        } else {
            PutChar(binfo, cursor_x, cursor_y, str[i], color, current_bg_color);
            cursor_x += 8;
        }

        /* 가로 화면 끝에 도달하면 자동 개행 */
        if (cursor_x + 8 > (int)binfo->horizontal_resolution) {
            cursor_x = 0;
            cursor_y += 16;
        }

        /* 세로 화면 끝에 도달하면 스크롤 */
        if (cursor_y + 16 > (int)binfo->vertical_resolution) {
            ScrollUp(binfo);
            cursor_y -= 16;
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

/**
 * UserMain: 실제 Ring 3 (User Mode)에서 실행될 테스트 함수
 */
void UserMain(int arg) {
    while(1) {
        printf("Hello from User Mode Task! Arg: %d\n", arg);

        /* 바쁜 대기 */
        for(volatile int i=0; i<100000000; i++);
    }
}


/*
 * InitializeFPU: FPU와 SSE 유닛을 활성화합니다.
 */
void InitializeFPU() {
    // 초기 상태 확인 (사용자 확인용)
    uint64_t initial_cr0 = ReadCR0();
    uint64_t initial_cr4 = ReadCR4();
    printf("Initial Processor State - CR0: %p, CR4: %p\n", (void*)initial_cr0, (void*)initial_cr4);

    // 1. CR0 레지스터 설정
    uint64_t cr0 = initial_cr0;
    cr0 &= ~(1 << 2); // EM (Emulation) bit 끄기
    cr0 |= (1 << 1);  // MP (Monitor Co-processor) bit 켜기
    WriteCR0(cr0);

    // 2. CR4 레지스터 설정 (SSE 활성화 포함)
    uint64_t cr4 = initial_cr4;
    cr4 |= (1 << 9);  // OSFXSR: FXSAVE/FXRSTOR 지원 및 SSE 활성화
    cr4 |= (1 << 10); // OSXMMEXCPT: 미마스크 SSE 예외 지원
    WriteCR4(cr4);

    // 3. FPU 초기화 (finit)
    InitFPU();

    Printf("FPU and SSE Initialized.\n");
}

/*
 * TestFPU: 부동소수점 연산 능력을 테스트합니다.
 */
void TestFPU(const char* context) {
    double a = 1.2345;
    double b = 2.7655;
    double res = a + b; // 기대값: 4.0

    // %f 지원 여부가 불확실하므로 수동으로 출력
    int int_part = (int)res;
    int frac_part = (int)((res - int_part) * 10000);
    
    printf("[%s] FPU Test: 1.2345 + 2.7655 = %d.%04d\n", context, int_part, frac_part);
    
    // %f 지원 여부 테스트 (newlib 설정에 따라 다름)
    // printf("[%s] FPU Test (%%f): %f\n", context, res);
}

/*
 * kmain: 커널 메인 로직
 */
void kmain(BootInfo *boot_info)
{
    boot_info_global = boot_info;

    /* 1. 화면 초기화 (진한 파란색 배경) */
    ClearScreen(boot_info, 0x00000033);

    /* 2. 프로세서 환경 설정 (GDT/IDT/Syscall) */
    Printf("Initializing System Tables (GDT/IDT/Syscall)...\n");
    InitGDT();
    InitIDT();
    InitSyscall();

    /* 3. 메모리 관리자 초기화 (PMM/VMM) */
    PMM_Init(boot_info);
    VMM_Init(boot_info);

    /* 4. 커널 힙 초기화 (기존 ToyOS 전용 힙) */
    Heap_Init(boot_info);

    /* 5. 환영 메시지 및 Newlib 테스트 */
    printf("\nWelcome to ToyOS! (UEFI 64-bit Mode)\n");
    printf("------------------------------------\n");
    
    InitializeFPU(); // FPU 활성화
    // FPU 커널 모드 테스트
    TestFPU("Kernel");
    
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
    PIT_Init(100);
    
    /* 7. 멀티태스킹 초기화 */
    InitializeTaskSystem();
    CreateTask(TaskB);
    
    /* 8. 유저 모드 테스트 태스크 생성 (격리 검증용으로 2개 생성) */
    printf("UserMain Address: %p\n", (void*)UserMain);
    //CreateUserTask(UserMain, 111); // 첫 번째 유저 태스크
    //CreateUserTask(UserMain, 222); // 두 번째 유저 태스크 (동일한 가상 주소 스택 사용)

    EnableInterrupts();
    printf("System Ready with Multitasking.\n");

    /* 9. 쉘 시작 */
    CreateTask(Shell_Main);

    printf("\nToyOS is now running with Shell support.\n");
    printf("Entering Task A loop...\n");

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
