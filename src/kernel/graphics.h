#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <stdint.h>
#include "kernel.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_LAYERS 256

/* Layer 구조체: 화면의 개별 계층(창, 마우스 등)을 정의합니다. */
typedef struct {
    uint32_t *buffer;           // 레이어 고유의 이미지 버퍼
    int x, y;                   // 화면 내 위치
    int width, height;          // 레이어 크기
    uint32_t transparent_color; // 투명하게 처리할 색상 (Color Key)
    int z_order;                // 겹침 순서 (높을수록 상단)
    int flags;                  // 레이어 속성 (예: 표시 여부)
    
    /* 터미널 기능 확장 */
    int cursor_x, cursor_y;     // 레이어 내 커서 위치
} Layer;

/* Window 구조체: 레이어를 포함하는 고수준 UI 구성 요소 */
typedef struct {
    Layer *layer;
    int is_dragging;
    int drag_x, drag_y;
} Window;

/* LayerManager 구조체: 모든 레이어를 관리하고 합성을 담당합니다. */
typedef struct {
    Layer *layers[MAX_LAYERS];
    int top;                    // 현재 레이어 개수
} LayerManager;

/* Graphics_Init: 그래픽 시스템 및 백 버퍼를 초기화합니다. */
void Graphics_Init(BootInfo *binfo);

/* SwapBuffers: 백 버퍼의 내용을 실제 프레임버퍼(VRAM)로 복사합니다. */
void SwapBuffers(BootInfo *binfo);

/* Layer 관련 함수 */
Layer* CreateLayer(int width, int height, uint32_t transparent_color);
void Layer_Move(Layer *layer, int x, int y);
void Layer_SetZOrder(Layer *layer, int z);
void Layer_DrawFillRect(Layer *layer, int x, int y, int width, int height, uint32_t color);
void Layer_PutChar(Layer *layer, int x, int y, char c, uint32_t color);
void Layer_PrintString(Layer *layer, int x, int y, const char *str, uint32_t color);

/* 터미널 에뮬레이션 기능 */
void Layer_TerminalPutChar(Layer *layer, char c, uint32_t color, uint32_t bg_color);
void Layer_TerminalPrintString(Layer *layer, const char *str, uint32_t color, uint32_t bg_color);
void Layer_ScrollUp(Layer *layer, uint32_t bg_color);

/* Window 관련 함수 */
Window* CreateWindow(int x, int y, int width, int height, const char *title);

/* LayerManager 관련 함수 */
void LayerManager_Init();
void LayerManager_AddLayer(Layer *layer);
void LayerManager_Render(BootInfo *binfo);
Layer* LayerManager_GetLayerAt(int x, int y);

/* 전역 쉘 레이어 설정 (syscall용) */
extern Layer *g_ShellLayer;

/* Draw API (백 버퍼 직접 그리기) */
void DrawPixel(BootInfo *binfo, int x, int y, uint32_t color);
void DrawRect(BootInfo *binfo, int x, int y, int width, int height, uint32_t color);
void DrawFillRect(BootInfo *binfo, int x, int y, int width, int height, uint32_t color);

#ifdef __cplusplus
}
#endif

#endif
