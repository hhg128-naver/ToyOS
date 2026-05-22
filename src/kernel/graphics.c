#include "graphics.h"
#include "heap.h"
#include "font.h"
#include <string.h>
#include <stdlib.h>

static uint32_t *back_buffer = NULL;
static uint64_t back_buffer_size = 0;
static LayerManager layer_manager;

void Graphics_Init(BootInfo *binfo)
{
    back_buffer_size = binfo->horizontal_resolution * binfo->vertical_resolution * sizeof(uint32_t);
    back_buffer = (uint32_t *)kmalloc(back_buffer_size);
    
    if (back_buffer != NULL)
    {
        memset(back_buffer, 0, back_buffer_size);
    }

    LayerManager_Init();
}

void SwapBuffers(BootInfo *binfo)
{
    if (back_buffer == NULL) return;
    memcpy(binfo->framebuffer, back_buffer, back_buffer_size);
}

/* --- Layer 구현 --- */

Layer* CreateLayer(int width, int height, uint32_t transparent_color)
{
    Layer *layer = (Layer *)kmalloc(sizeof(Layer));
    if (!layer) return NULL;

    layer->buffer = (uint32_t *)kmalloc(width * height * sizeof(uint32_t));
    if (!layer->buffer)
    {
        kfree(layer);
        return NULL;
    }

    layer->width = width;
    layer->height = height;
    layer->x = 0;
    layer->y = 0;
    layer->transparent_color = transparent_color;
    layer->z_order = 0;
    layer->flags = 1; // Visible

    for (int i = 0; i < width * height; i++)
    {
        layer->buffer[i] = transparent_color;
    }

    return layer;
}

void Layer_Move(Layer *layer, int x, int y)
{
    layer->x = x;
    layer->y = y;
}

void Layer_SetZOrder(Layer *layer, int z)
{
    layer->z_order = z;
}

void Layer_DrawFillRect(Layer *layer, int x, int y, int width, int height, uint32_t color)
{
    for (int dy = 0; dy < height; dy++)
    {
        int ny = y + dy;
        if (ny < 0 || ny >= layer->height) continue;
        for (int dx = 0; dx < width; dx++)
        {
            int nx = x + dx;
            if (nx < 0 || nx >= layer->width) continue;
            layer->buffer[ny * layer->width + nx] = color;
        }
    }
}

void Layer_PutChar(Layer *layer, int x, int y, char c, uint32_t color)
{
    unsigned char *font_ptr = &EnglishFont[(unsigned char)c * 16];
    for (int dy = 0; dy < 16; dy++)
    {
        for (int dx = 0; dx < 8; dx++)
        {
            if ((font_ptr[dy] << dx) & 0x80)
            {
                int nx = x + dx;
                int ny = y + dy;
                if (nx >= 0 && nx < layer->width && ny >= 0 && ny < layer->height)
                {
                    layer->buffer[ny * layer->width + nx] = color;
                }
            }
        }
    }
}

void Layer_PrintString(Layer *layer, int x, int y, const char *str, uint32_t color)
{
    for (int i = 0; str[i] != '\0'; i++)
    {
        Layer_PutChar(layer, x + i * 8, y, str[i], color);
    }
}

/* --- Window 구현 --- */

Window* CreateWindow(int x, int y, int width, int height, const char *title)
{
    Window *win = (Window *)kmalloc(sizeof(Window));
    if (!win) return NULL;

    win->layer = CreateLayer(width, height, 0xFF000001); // Magic transparent color
    if (!win->layer)
    {
        kfree(win);
        return NULL;
    }

    win->layer->x = x;
    win->layer->y = y;
    win->is_dragging = 0;

    // 창 배경 및 타이틀 바 그리기
    Layer_DrawFillRect(win->layer, 0, 0, width, height, 0x00C6C6C6);      // 창 배경
    Layer_DrawFillRect(win->layer, 0, 0, width, 25, 0x00000080);       // 타이틀 바 (파란색)
    Layer_PrintString(win->layer, 5, 5, title, 0x00FFFFFF);             // 제목 출력

    LayerManager_AddLayer(win->layer);
    return win;
}

/* --- LayerManager 구현 --- */

void LayerManager_Init()
{
    layer_manager.top = 0;
    for (int i = 0; i < MAX_LAYERS; i++)
    {
        layer_manager.layers[i] = NULL;
    }
}

void LayerManager_AddLayer(Layer *layer)
{
    if (layer_manager.top >= MAX_LAYERS) return;
    layer_manager.layers[layer_manager.top] = layer;
    layer_manager.top++;
}

Layer* LayerManager_GetLayerAt(int x, int y)
{
    /* 상단 레이어부터 검색 (높은 인덱스/Z-order) */
    for (int i = layer_manager.top - 1; i >= 0; i--)
    {
        Layer *layer = layer_manager.layers[i];
        if (!layer || !(layer->flags & 1)) continue;

        // 마우스 커서 레이어(보통 Z-order 1000)는 창 감지에서 제외
        if (layer->z_order >= 1000) continue;

        if (x >= layer->x && x < layer->x + layer->width &&
            y >= layer->y && y < layer->y + layer->height)
        {
            // 투명 영역 체크
            int lx = x - layer->x;
            int ly = y - layer->y;
            if (layer->buffer[ly * layer->width + lx] != layer->transparent_color)
            {
                return layer;
            }
        }
    }
    return NULL;
}

void LayerManager_Render(BootInfo *binfo)
{
    if (back_buffer == NULL) return;
    memset(back_buffer, 0, back_buffer_size);

    int screen_w = binfo->horizontal_resolution;
    int screen_h = binfo->vertical_resolution;

    // Z-order 정렬
    for (int i = 0; i < layer_manager.top - 1; i++)
    {
        for (int j = i + 1; j < layer_manager.top; j++)
        {
            if (layer_manager.layers[i]->z_order > layer_manager.layers[j]->z_order)
            {
                Layer *temp = layer_manager.layers[i];
                layer_manager.layers[i] = layer_manager.layers[j];
                layer_manager.layers[j] = temp;
            }
        }
    }

    for (int i = 0; i < layer_manager.top; i++)
    {
        Layer *layer = layer_manager.layers[i];
        if (!layer || !(layer->flags & 1)) continue;

        for (int ly = 0; ly < layer->height; ly++)
        {
            int sy = layer->y + ly;
            if (sy < 0 || sy >= screen_h) continue;
            for (int lx = 0; lx < layer->width; lx++)
            {
                int sx = layer->x + lx;
                if (sx < 0 || sx >= screen_w) continue;
                uint32_t color = layer->buffer[ly * layer->width + lx];
                if (color != layer->transparent_color)
                {
                    back_buffer[sy * screen_w + sx] = color;
                }
            }
        }
    }
}

/* --- Direct Draw API (하위 호환) --- */
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
    for (int dy = 0; dy < height; dy++)
    {
        int ny = y + dy;
        if (ny < 0 || ny >= (int)binfo->vertical_resolution) continue;
        for (int dx = 0; dx < width; dx++)
        {
            int nx = x + dx;
            if (nx < 0 || nx >= (int)binfo->horizontal_resolution) continue;
            back_buffer[ny * binfo->horizontal_resolution + nx] = color;
        }
    }
}
