#ifndef MOUSE_H
#define MOUSE_H

#include <stdint.h>
#include "kernel.h"

#define MOUSE_PORT_DATA    0x60
#define MOUSE_PORT_STATUS  0x64
#define MOUSE_PORT_COMMAND 0x64

/* 마우스 버튼 비트 */
#define MOUSE_LEFT_BUTTON   0x01
#define MOUSE_RIGHT_BUTTON  0x02
#define MOUSE_MIDDLE_BUTTON 0x04

typedef struct
{
    int x;
    int y;
    uint8_t buttons;
    int8_t scroll;          /* 마우스 휠 스크롤 델타 (양수=위, 음수=아래) */
} MouseState;

void Mouse_Init(BootInfo *binfo);
void Mouse_Handler();
MouseState GetMouseState();
void Mouse_SetSensitivity(int sensitivity);

/**
 * Mouse_GetScroll: 마우스 휠 스크롤 델타를 반환하고 내부 값을 리셋합니다.
 * @return 스크롤 델타 (양수=위로 스크롤, 음수=아래로 스크롤, 0=변화 없음)
 */
int8_t Mouse_GetScroll(void);

#endif
