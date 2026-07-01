#include "graphics.h"
#include "heap.h"
#include "font.h"
#include "mouse.h"
#include "task.h"
#include "console.h"
#include <string.h>
#include <stdlib.h>

static uint32_t *back_buffer = nullptr;
static uint64_t back_buffer_size = 0;
static LayerManager layer_manager;
Layer *g_ShellLayer = nullptr;
Window *shell_win = nullptr;

void Graphics_Init(BootInfo *binfo)
{
    back_buffer_size = binfo->horizontal_resolution * binfo->vertical_resolution * sizeof(uint32_t);
    back_buffer = (uint32_t *)kmalloc(back_buffer_size);
    
    if (back_buffer != nullptr)
    {
        memset(back_buffer, 0, back_buffer_size);
    }

    LayerManager_Init();
}

void SwapBuffers(BootInfo *binfo)
{
    if (back_buffer == nullptr) return;
    memcpy(binfo->framebuffer, back_buffer, back_buffer_size);
}

/* --- Layer 구현 --- */

Layer* CreateLayer(int width, int height, uint32_t transparent_color)
{
    Layer *layer = (Layer *)kmalloc(sizeof(Layer));
    if (!layer) return nullptr;

    layer->buffer = (uint32_t *)kmalloc(width * height * sizeof(uint32_t));
    if (!layer->buffer)
    {
        kfree(layer);
        return nullptr;
    }

    layer->width = width;
    layer->height = height;
    layer->x = 0;
    layer->y = 0;
    layer->transparent_color = transparent_color;
    layer->z_order = 0;
    layer->flags = 1; // Visible
    layer->cursor_x = 0;
    layer->cursor_y = 0;

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

/* --- 터미널 에뮬레이션 구현 --- */

void Layer_ScrollUp(Layer *layer, uint32_t bg_color)
{
    int width = layer->width;
    int height = layer->height;
    int line_height = 16;

    // 첫 줄 제외하고 위로 복사
    uint32_t *dest = layer->buffer;
    uint32_t *src = layer->buffer + line_height * width;
    uint64_t copy_size = (uint64_t)(height - line_height) * width * sizeof(uint32_t);
    
    memmove(dest, src, copy_size);

    // 마지막 줄 초기화
    uint32_t *last_line = layer->buffer + (uint64_t)(height - line_height) * width;
    for (uint64_t i = 0; i < (uint64_t)line_height * width; i++)
    {
        last_line[i] = bg_color;
    }
}

void Layer_TerminalPutChar(Layer *layer, char c, uint32_t color, uint32_t bg_color)
{
    if (c == '\n')
    {
        layer->cursor_x = 0;
        layer->cursor_y += 16;
    }
    else if (c == '\b')
    {
        if (layer->cursor_x >= 8)
        {
            layer->cursor_x -= 8;
            Layer_DrawFillRect(layer, layer->cursor_x, layer->cursor_y, 8, 16, bg_color);
        }
    }
    else
    {
        // 배경을 먼저 칠해줌 (글자 겹침 방지)
        Layer_DrawFillRect(layer, layer->cursor_x, layer->cursor_y, 8, 16, bg_color);
        Layer_PutChar(layer, layer->cursor_x, layer->cursor_y, c, color);
        layer->cursor_x += 8;
    }

    if (layer->cursor_x + 8 > layer->width)
    {
        layer->cursor_x = 0;
        layer->cursor_y += 16;
    }

    if (layer->cursor_y + 16 > layer->height)
    {
        Layer_ScrollUp(layer, bg_color);
        layer->cursor_y -= 16;
    }
}

void Layer_TerminalPrintString(Layer *layer, const char *str, uint32_t color, uint32_t bg_color)
{
    for (int i = 0; str[i] != '\0'; i++)
    {
        Layer_TerminalPutChar(layer, str[i], color, bg_color);
    }
}

/* --- Window 구현 --- */

Window* CreateWindow(int x, int y, int width, int height, const char *title)
{
    Window *win = (Window *)kmalloc(sizeof(Window));
    if (!win) return nullptr;

    win->layer = CreateLayer(width, height, 0xFF000001); 
    if (!win->layer)
    {
        kfree(win);
        return nullptr;
    }

    win->layer->x = x;
    win->layer->y = y;
    win->is_dragging = 0;

    Layer_DrawFillRect(win->layer, 0, 0, width, height, 0x00C6C6C6);      // 창 배경
    Layer_DrawFillRect(win->layer, 0, 0, width, 25, 0x00000080);       // 타이틀 바
    Layer_PrintString(win->layer, 5, 5, title, 0x00FFFFFF);             

    // 터미널 시작 위치 설정 (타이틀 바 아래)
    win->layer->cursor_x = 5;
    win->layer->cursor_y = 30;

    LayerManager_AddLayer(win->layer);
    return win;
}

/* --- LayerManager 구현 --- */

void LayerManager_Init()
{
    layer_manager.top = 0;
    for (int i = 0; i < MAX_LAYERS; i++)
    {
        layer_manager.layers[i] = nullptr;
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
    for (int i = layer_manager.top - 1; i >= 0; i--)
    {
        Layer *layer = layer_manager.layers[i];
        if (!layer || !(layer->flags & 1)) continue;
        if (layer->z_order >= 1000) continue;
        if (x >= layer->x && x < layer->x + layer->width &&
            y >= layer->y && y < layer->y + layer->height)
        {
            int lx = x - layer->x;
            int ly = y - layer->y;
            if (layer->buffer[ly * layer->width + lx] != layer->transparent_color)
            {
                return layer;
            }
        }
    }
    return nullptr;
}

void LayerManager_Render(BootInfo *binfo)
{
    if (back_buffer == nullptr) return;
    memset(back_buffer, 0, back_buffer_size);

    int screen_w = binfo->horizontal_resolution;
    int screen_h = binfo->vertical_resolution;

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

/* --- Direct Draw API --- */
void DrawPixel(BootInfo *binfo, int x, int y, uint32_t color)
{
    if (back_buffer == nullptr) return;
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
    if (back_buffer == nullptr) return;
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

void GUI_Task(void)
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
