#include "mouse.h"
#include "graphics.h"
#include <stdio.h>

extern void outb(uint16_t port, uint8_t data);
extern uint8_t inb(uint16_t port);

static BootInfo *mouse_boot_info = NULL;
static MouseState current_mouse_state = {400, 300, 0};
static uint8_t mouse_cycle = 0;
static uint8_t mouse_packet[3];

/* 마우스 감도 */
static int mouse_sensitivity = 2;

/* 마우스 커서 레이어 */
static Layer *mouse_layer = NULL;

/* 화살표 모양의 마우스 커서 비트맵 (12x19) */
static const uint16_t mouse_cursor_bitmap[19] = {
    0b1100000000000000,
    0b1110000000000000,
    0b1111000000000000,
    0b1111100000000000,
    0b1111110000000000,
    0b1111111000000000,
    0b1111111100000000,
    0b1111111110000000,
    0b1111111111000000,
    0b1111111111100000,
    0b1111111111110000,
    0b1111111000000000,
    0b1101111000000000,
    0b1000111000000000,
    0b0000011100000000,
    0b0000011100000000,
    0b0000001110000000,
    0b0000001110000000,
    0b0000000000000000
};

/* PS/2 컨트롤러 대기 */
static void Mouse_Wait(uint8_t type)
{
    uint32_t timeout = 100000;
    if (type == 0)
    {
        while (timeout--)
        {
            if ((inb(MOUSE_PORT_STATUS) & 1) == 1)
            {
                return;
            }
        }
    }
    else
    {
        while (timeout--)
        {
            if ((inb(MOUSE_PORT_STATUS) & 2) == 0)
            {
                return;
            }
        }
    }
}

/* 마우스 명령 전송 */
static void Mouse_Write(uint8_t data)
{
    Mouse_Wait(1);
    outb(MOUSE_PORT_COMMAND, 0xD4);
    Mouse_Wait(1);
    outb(MOUSE_PORT_DATA, data);
}

/* 마우스 데이터 읽기 */
static uint8_t Mouse_Read()
{
    Mouse_Wait(0);
    return inb(MOUSE_PORT_DATA);
}

void Mouse_Init(BootInfo *binfo)
{
    mouse_boot_info = binfo;

    uint8_t status;

    Mouse_Wait(1);
    outb(MOUSE_PORT_COMMAND, 0xA8);

    Mouse_Wait(1);
    outb(MOUSE_PORT_COMMAND, 0x20);
    Mouse_Wait(0);
    status = (inb(MOUSE_PORT_DATA) | 2);
    Mouse_Wait(1);
    outb(MOUSE_PORT_COMMAND, 0x60);
    Mouse_Wait(1);
    outb(MOUSE_PORT_DATA, status);

    Mouse_Write(0xF6);
    Mouse_Read();

    Mouse_Write(0xF4);
    Mouse_Read();

    /* 마우스 커서 레이어 생성 (투명색: 0x00000000) */
    mouse_layer = CreateLayer(12, 19, 0x00000000);
    if (mouse_layer)
    {
        for (int dy = 0; dy < 19; dy++)
        {
            for (int dx = 0; dx < 12; dx++)
            {
                if ((mouse_cursor_bitmap[dy] >> (15 - dx)) & 0x01)
                {
                    mouse_layer->buffer[dy * 12 + dx] = 0x00FFFFFF; // 흰색 커서
                }
                else
                {
                    mouse_layer->buffer[dy * 12 + dx] = 0x00000000; // 투명
                }
            }
        }
        Layer_Move(mouse_layer, current_mouse_state.x, current_mouse_state.y);
        Layer_SetZOrder(mouse_layer, 1000); // 항상 최상단에 배치
        LayerManager_AddLayer(mouse_layer);
    }
}

void Mouse_Handler()
{
    uint8_t status = inb(MOUSE_PORT_STATUS);
    
    if ((status & 1) && (status & 0x20))
    {
        mouse_packet[mouse_cycle++] = inb(MOUSE_PORT_DATA);

        if (mouse_cycle == 3)
        {
            mouse_cycle = 0;

            current_mouse_state.buttons = mouse_packet[0] & 0x07;

            int32_t x_offset = (int32_t)mouse_packet[1];
            int32_t y_offset = (int32_t)mouse_packet[2];

            if (mouse_packet[0] & 0x10) x_offset |= 0xFFFFFF00;
            if (mouse_packet[0] & 0x20) y_offset |= 0xFFFFFF00;

            // 좌표 업데이트
            current_mouse_state.x += (x_offset / mouse_sensitivity);
            current_mouse_state.y -= (y_offset / mouse_sensitivity);

            if (current_mouse_state.x < 0) current_mouse_state.x = 0;
            if (current_mouse_state.y < 0) current_mouse_state.y = 0;
            if (current_mouse_state.x >= (int)mouse_boot_info->horizontal_resolution)
                current_mouse_state.x = mouse_boot_info->horizontal_resolution - 1;
            if (current_mouse_state.y >= (int)mouse_boot_info->vertical_resolution)
                current_mouse_state.y = mouse_boot_info->vertical_resolution - 1;
            
            // 레이어 위치 업데이트
            if (mouse_layer)
            {
                Layer_Move(mouse_layer, current_mouse_state.x, current_mouse_state.y);
            }
        }
    }
}

MouseState GetMouseState()
{
    return current_mouse_state;
}

void Mouse_SetSensitivity(int sensitivity)
{
    if (sensitivity < 1) sensitivity = 1;
    mouse_sensitivity = sensitivity;
}
