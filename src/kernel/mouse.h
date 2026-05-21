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
} MouseState;

void Mouse_Init(BootInfo *binfo);
void Mouse_Handler();
MouseState GetMouseState();
void Mouse_SetSensitivity(int sensitivity);

#endif
