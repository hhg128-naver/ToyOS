#include "console.h"
#include "font.h"

/*
 * 그래픽 콘솔을 위한 전역 상태 변수
 */
static int cursor_x = 0;
static int cursor_y = 0;
BootInfo *boot_info_global = (void*)0;
static uint32_t current_bg_color = 0x00000033; // 현재 시스템 배경색

void Console_Init(BootInfo *binfo)
{
    boot_info_global = binfo;
    ClearScreen(binfo, 0x00000033);
}

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

void PrintStringLen(BootInfo *binfo, const char *str, uint32_t len, uint32_t color)
{
    for (uint32_t i = 0; i < len; i++)
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
