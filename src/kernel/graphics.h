#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <stdint.h>
#include "kernel.h"

#ifdef __cplusplus
extern "C" {
#endif

/* DrawPixel: 특정 위치에 점을 찍습니다. */
void DrawPixel(BootInfo *binfo, int x, int y, uint32_t color);

/* DrawRect: 테두리만 있는 네모를 그립니다. */
void DrawRect(BootInfo *binfo, int x, int y, int width, int height, uint32_t color);

/* DrawFillRect: 색이 채워진 네모를 그립니다. */
void DrawFillRect(BootInfo *binfo, int x, int y, int width, int height, uint32_t color);

#ifdef __cplusplus
}
#endif

#endif
