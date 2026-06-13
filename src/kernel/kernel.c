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
#include "apic.h"
#include "mp.h"
#include "acpi.h"
#include "fpu.h"
#include "smp.h"

/* 어셈블리(asm_utils.asm)에서 정의됨 */
extern void EnableInterrupts();
extern void call_constructors();

void kmain(BootInfo *boot_info)
{
    Console_Init(boot_info);

    InitGDT();
    InitIDT();
    InitSyscall();

	// Physical Memory Manager와 Virtual Memory Manager 초기화는 GDT/IDT 설정 이후에 수행되어야 합니다.
    PMM_Init(boot_info);
    VMM_Init(boot_info);

    Heap_Init(boot_info);

	// c++ 전역 객체의 생성자를 호출하여 초기화 작업 수행 (예: std::string, std::vector 등)
    call_constructors();

    InitializeFPU();

    PIC_Init();

    /* MP Configuration Table 탐색 (멀티프로세서 정보 수집) */
    /* Deprecated: MP_Init()는 더 이상 사용되지 않음 */
    MP_Init();

    /* ACPI MADT 파싱 (인터럽트 컨트롤러 토폴로지 수집) */
    ACPI_Init(boot_info);

    /* Local APIC 활성화 및 APIC Timer로 PIT 대체 */
    APIC_Init();
    APIC_Timer_Init(100); /* 100Hz (10ms 주기) */
    PIC_MaskIRQ(PIC_IRQ_PIT); /* PIT(IRQ0) 마스킹 — APIC Timer가 타이머 역할 수행 */

    /* AP들을 깨우고 온라인 상태를 확인 (INIT-SIPI-SIPI 프로토콜) */
    SMP_Init();

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
    //g_ShellLayer = shell_win->layer;

    //CreateTask(GUI_Task);
    CreateTask(Shell_Main);

    printf("\nToyOS Graphical Shell is now active.\n");

    while (1)
    {
        __asm__ volatile("hlt");
        __asm__ volatile("hlt");
    }
}
