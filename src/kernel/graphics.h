#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <stdint.h>
#include "kernel.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Graphics_Init: 그래픽 시스템 및 백 버퍼를 초기화합니다. */
void Graphics_Init(BootInfo *binfo);

/* SwapBuffers: 백 버퍼의 내용을 실제 프레임버퍼(VRAM)로 복사합니다. */
void SwapBuffers(BootInfo *binfo);

/* DrawPixel: 백 버퍼의 특정 위치에 점을 찍습니다. */
void DrawPixel(BootInfo *binfo, int x, int y, uint32_t color);

/* DrawRect: 테두리만 있는 네모를 그립니다. */
void DrawRect(BootInfo *binfo, int x, int y, int width, int height, uint32_t color);

/* DrawFillRect: 색이 채워진 네모를 그립니다. */
void DrawFillRect(BootInfo *binfo, int x, int y, int width, int height, uint32_t color);

#ifdef __cplusplus
}
#endif

#endif
