#ifndef CONSOLE_H
#define CONSOLE_H

#include <stdint.h>
#include "kernel.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 전역 부트 정보 포인터 */
extern BootInfo *boot_info_global;

/* --- 그래픽 콘솔 기능 --- */

/* Console_Init: 콘솔 시스템을 초기화합니다. */
void Console_Init(BootInfo *binfo);

/* PutChar: 특정 위치(x, y)에 한 문자를 출력합니다. 배경색을 포함하여 잔상을 제거합니다. */
void kPutChar(BootInfo *binfo, int x, int y, char c, uint32_t color, uint32_t bg_color);

/* PrintString: 현재 커서 위치부터 문자열을 출력합니다. */
void kPrintString(BootInfo *binfo, const char *str, uint32_t color);

/* PrintStringLen: 지정된 길이만큼 문자열을 출력합니다. */
void PrintStringLen(BootInfo *binfo, const char *str, uint32_t len, uint32_t color);

/* ClearScreen: 화면을 특정 색상으로 초기화합니다. */
void ClearScreen(BootInfo *binfo, uint32_t color);

/* Printf: 가변 인자 없이 문자열만 출력하는 임시 함수 */
void kPrintf(const char *str);

#ifdef __cplusplus
}
#endif

#endif
