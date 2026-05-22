#include "graphics.h"

void DrawPixel(BootInfo *binfo, int x, int y, uint32_t color)
{
    if (x < 0 || x >= (int)binfo->horizontal_resolution || y < 0 || y >= (int)binfo->vertical_resolution)
        return;

    binfo->framebuffer[y * binfo->horizontal_resolution + x] = color;
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
    uint32_t *vidptr = binfo->framebuffer;
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

            vidptr[ny * res_x + nx] = color;
        }
    }
}
