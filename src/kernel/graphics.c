#include "graphics.h"
#include "heap.h"
#include <string.h>

static uint32_t *back_buffer = NULL;
static uint64_t back_buffer_size = 0;

void Graphics_Init(BootInfo *binfo)
{
    back_buffer_size = binfo->horizontal_resolution * binfo->vertical_resolution * sizeof(uint32_t);
    back_buffer = (uint32_t *)kmalloc(back_buffer_size);
    
    if (back_buffer != NULL)
    {
        memset(back_buffer, 0, back_buffer_size);
    }
}

void SwapBuffers(BootInfo *binfo)
{
    if (back_buffer == NULL) return;

    /* 백 버퍼의 내용을 실제 프레임버퍼(VRAM)로 복사 */
    memcpy(binfo->framebuffer, back_buffer, back_buffer_size);
}

void DrawPixel(BootInfo *binfo, int x, int y, uint32_t color)
{
    if (back_buffer == NULL) return;
    if (x < 0 || x >= (int)binfo->horizontal_resolution || y < 0 || y >= (int)binfo->vertical_resolution)
        return;

    back_buffer[y * binfo->horizontal_resolution + x] = color;
}

void DrawRect(BootInfo *binfo, int x, int y, int width, int height, uint32_t color)
{
    for (int i = 0; i < width; i++)
    {
        DrawPixel(binfo, x + i, y, color);
        DrawPixel(binfo, x + i, y + height - 1, color);
    }
    for (int i = 0; i < height; i++)
    {
        DrawPixel(binfo, x, y + i, color);
        DrawPixel(binfo, x + width - 1, y + i, color);
    }
}

void DrawFillRect(BootInfo *binfo, int x, int y, int width, int height, uint32_t color)
{
    if (back_buffer == NULL) return;
    
    uint32_t res_x = binfo->horizontal_resolution;
    uint32_t res_y = binfo->vertical_resolution;

    for (int dy = 0; dy < height; dy++)
    {
        int ny = y + dy;
        if (ny < 0 || ny >= (int)res_y) continue;

        for (int dx = 0; dx < width; dx++)
        {
            int nx = x + dx;
            if (nx < 0 || nx >= (int)res_x) continue;

            back_buffer[ny * res_x + nx] = color;
        }
    }
}
