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
    Mouse_Init(boot_info);

    Graphics_Init(boot_info);

    EnableInterrupts();
    printf("System Ready with Multitasking and Mouse support.\n");

    /* 그래픽 기능 테스트 */
    DrawFillRect(boot_info, 100, 100, 200, 150, 0x00FF0000); // 빨간색 채워진 네모
    DrawFillRect(boot_info, 150, 150, 200, 150, 0x0000FF00); // 초록색 채워진 네모
    DrawFillRect(boot_info, 200, 200, 200, 150, 0x000000FF); // 파란색 채워진 네모
    DrawRect(boot_info, 80, 80, 340, 290, 0x00FFFFFF);      // 흰색 테두리 네모
    SwapBuffers(boot_info);

    CreateTask(Shell_Main);

    printf("\nToyOS is now running with Shell support.\n");
    printf("Entering A loop...\n");

    while (1)
    {
        __asm__ volatile("hlt");
    }
}
