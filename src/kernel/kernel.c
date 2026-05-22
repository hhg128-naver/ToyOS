#include <stdio.h>
#include <stdlib.h>
#include "kernel.h"
#include "console.h"
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
#include "ide.h"
#include "vfs.h"
#include "fat32.h"
#include "mouse.h"
#include "graphics.h"

/* 어셈블리(asm_utils.asm)에서 정의됨 */
extern void EnableInterrupts();
extern void call_constructors();

void InitializeFPU()
{
    uint64_t initial_cr0 = ReadCR0();
    uint64_t initial_cr4 = ReadCR4();
    printf("Initial Processor State - CR0: %p, CR4: %p\n", (void *)initial_cr0, (void *)initial_cr4);

    uint64_t cr0 = initial_cr0;
    cr0 &= ~(1 << 2);
    cr0 |= (1 << 1);
    WriteCR0(cr0);

    uint64_t cr4 = initial_cr4;
    cr4 |= (1 << 9);
    cr4 |= (1 << 10);
    WriteCR4(cr4);

    InitFPU();
    Printf("FPU and SSE Initialized.\n");
}

/* 윈도우 목록 관리 */
static Window *shell_win = NULL;

/* GUI_Task: 화면 갱신 및 마우스 이벤트 처리 */
void GUI_Task()
{
    while (1)
    {
        MouseState ms = GetMouseState();
        
        // 마우스 상호작용 (드래그 로직)
        if (ms.buttons & MOUSE_LEFT_BUTTON)
        {
            if (shell_win && !shell_win->is_dragging)
            {
                // 타이틀 바 영역(높이 25) 내에서 클릭했는지 확인
                if (ms.x >= shell_win->layer->x && ms.x < shell_win->layer->x + shell_win->layer->width &&
                    ms.y >= shell_win->layer->y && ms.y < shell_win->layer->y + 25)
                {
                    shell_win->is_dragging = 1;
                    shell_win->drag_x = ms.x - shell_win->layer->x;
                    shell_win->drag_y = ms.y - shell_win->layer->y;
                    
                    Layer_SetZOrder(shell_win->layer, 900);
                }
            }
            
            if (shell_win && shell_win->is_dragging)
            {
                Layer_Move(shell_win->layer, ms.x - shell_win->drag_x, ms.y - shell_win->drag_y);
            }
        }
        else
        {
            if (shell_win) shell_win->is_dragging = 0;
        }

        LayerManager_Render(boot_info_global);
        SwapBuffers(boot_info_global);
        Yield(); 
    }
}

void kmain(BootInfo *boot_info)
{
    Console_Init(boot_info);

    InitGDT();
    InitIDT();
    InitSyscall();

    PMM_Init(boot_info);
    VMM_Init(boot_info);

    Heap_Init(boot_info);

    call_constructors();

    InitializeFPU();

    PIC_Init();
    PIT_Init(100);

    InitializeTaskSystem();

    IDE_Init();
    FAT32_Init();
    
    Graphics_Init(boot_info);
    Mouse_Init(boot_info);

    EnableInterrupts();
    printf("System Ready with Interactive Windowing System.\n");

    /* 배경 레이어 생성 */
    Layer *bg_layer = CreateLayer(boot_info->horizontal_resolution, boot_info->vertical_resolution, 0xFF000001);
    Layer_DrawFillRect(bg_layer, 0, 0, boot_info->horizontal_resolution, boot_info->vertical_resolution, 0x00000033);
    Layer_PrintString(bg_layer, 10, boot_info->vertical_resolution - 20, "ToyOS v0.1 - Terminal Emulation Mode", 0x00FFFFFF);
    LayerManager_AddLayer(bg_layer);

    /* 쉘 윈도우 생성 */
    shell_win = CreateWindow(100, 100, 640, 480, "Terminal - Shell");
    g_ShellLayer = shell_win->layer;

    CreateTask(GUI_Task);
    CreateTask(Shell_Main);

    printf("\nToyOS Graphical Shell is now active.\n");

    while (1)
    {
        __asm__ volatile("hlt");
    }
}
