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
#include "ide.h"
#include "vfs.h"
#include "fat32.h"
#include "mouse.h"

/* 어셈블리(asm_utils.asm)에서 정의됨 */
extern void EnableInterrupts();
extern void call_constructors();

/*
 * 그래픽 콘솔을 위한 전역 상태 변수
 */
static int cursor_x = 0;
static int cursor_y = 0;
BootInfo *boot_info_global;
static uint32_t current_bg_color = 0x00000033; // 현재 시스템 배경색

void Printf(const char *str)
{
    PrintString(boot_info_global, str, 0x00FFFFFF);
}

void ClearScreen(BootInfo *binfo, uint32_t color)
{
    uint32_t *vidptr = binfo->framebuffer;
    for (uint64_t i = 0; i < binfo->screen_size; i++)
    {
        vidptr[i] = color;
    }
    cursor_x = 0;
    cursor_y = 0;
    current_bg_color = color;
}

void PutChar(BootInfo *binfo, int x, int y, char c, uint32_t color, uint32_t bg_color)
{
    unsigned char *font_ptr = &EnglishFont[(unsigned char)c * 16];
    uint32_t *vidptr = binfo->framebuffer;

    for (int dy = 0; dy < 16; dy++)
    {
        for (int dx = 0; dx < 8; dx++)
        {
            if ((font_ptr[dy] << dx) & 0x80)
            {
                vidptr[(y + dy) * binfo->horizontal_resolution + (x + dx)] = color;
            }
            else
            {
                vidptr[(y + dy) * binfo->horizontal_resolution + (x + dx)] = bg_color;
            }
        }
    }
}

static void ScrollUp(BootInfo *binfo)
{
    uint32_t *vidptr = binfo->framebuffer;
    uint32_t width = binfo->horizontal_resolution;
    uint32_t height = binfo->vertical_resolution;

    uint64_t scroll_pixels = (uint64_t)(height - 16) * width;
    uint64_t offset_pixels = (uint64_t)16 * width;

    for (uint64_t i = 0; i < scroll_pixels; i++)
    {
        vidptr[i] = vidptr[i + offset_pixels];
    }

    uint64_t total_pixels = (uint64_t)height * width;
    for (uint64_t i = scroll_pixels; i < total_pixels; i++)
    {
        vidptr[i] = current_bg_color;
    }
}

void PrintString(BootInfo *binfo, const char *str, uint32_t color)
{
    for (int i = 0; str[i] != '\0'; i++)
    {
        if (str[i] == '\n')
        {
            cursor_x = 0;
            cursor_y += 16;
        }
        else if (str[i] == '\b')
        {
            if (cursor_x >= 8)
            {
                cursor_x -= 8;
                PutChar(binfo, cursor_x, cursor_y, ' ', current_bg_color, current_bg_color);
            }
        }
        else
        {
            PutChar(binfo, cursor_x, cursor_y, str[i], color, current_bg_color);
            cursor_x += 8;
        }

        if (cursor_x + 8 > (int)binfo->horizontal_resolution)
        {
            cursor_x = 0;
            cursor_y += 16;
        }

        if (cursor_y + 16 > (int)binfo->vertical_resolution)
        {
            ScrollUp(binfo);
            cursor_y -= 16;
        }
    }
}

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
    boot_info_global = boot_info;

    ClearScreen(boot_info, 0x00000033);

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

    EnableInterrupts();
    printf("System Ready with Multitasking and Mouse support.\n");

    CreateTask(Shell_Main);

    printf("\nToyOS is now running with Shell support.\n");
    printf("Entering A loop...\n");

    while (1)
    {
        __asm__ volatile("hlt");
    }
}
