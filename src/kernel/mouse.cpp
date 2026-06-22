#include "mouse.h"
#include "graphics.h"
#include <stdio.h>




static BootInfo *mouse_boot_info = NULL;
static MouseState current_mouse_state = {400, 300, 0, 0};
static uint8_t mouse_cycle = 0;
static uint8_t mouse_packet[4];

/* 마우스 감도 */
static int mouse_sensitivity = 2;

/* 마우스 커서 레이어 */
static Layer *mouse_layer = NULL;

/* IntelliMouse 모드 여부 (4바이트 패킷) */
static int mouse_has_wheel = 0;

/* 스크롤 델타 누적 (인터럽트에서 쓰고 메인에서 읽음) */
static volatile int8_t scroll_delta = 0;

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

/*
 * Mouse_SetSampleRate: 마우스의 Sample Rate를 설정합니다.
 * IntelliMouse 확장 활성화를 위한 매직 시퀀스에 사용됩니다.
 *
 * @param rate: Sample Rate (Hz)
 */
static void Mouse_SetSampleRate(uint8_t rate)
{
    Mouse_Write(0xF3);   /* Set Sample Rate 명령 */
    Mouse_Read();         /* ACK */
    Mouse_Write(rate);
    Mouse_Read();         /* ACK */
}

/*
 * Mouse_EnableIntelliMouse: IntelliMouse 확장 프로토콜을 활성화합니다.
 *
 * 매직 시퀀스: Sample Rate를 200 → 100 → 80 순서로 설정하면
 * 마우스가 Mouse ID를 0x03으로 변경하고 4바이트 패킷 모드로 전환됩니다.
 * 4번째 바이트에 Z축(스크롤 휠) 데이터가 포함됩니다.
 *
 * @return 1이면 IntelliMouse 모드 활성화, 0이면 실패
 */
static int Mouse_EnableIntelliMouse(void)
{
    /* 매직 시퀀스: 200, 100, 80 */
    Mouse_SetSampleRate(200);
    Mouse_SetSampleRate(100);
    Mouse_SetSampleRate(80);

    /* Mouse ID 확인 */
    Mouse_Write(0xF2);  /* Get Mouse ID */
    Mouse_Read();        /* ACK */
    uint8_t mouse_id = Mouse_Read();

    if (mouse_id == 0x03)
    {
        kPrintf("Mouse: IntelliMouse wheel mode enabled (ID=0x%02x)\n", mouse_id);
        return 1;
    }
    else
    {
        kPrintf("Mouse: Standard mode (ID=0x%02x), no wheel support\n", mouse_id);
        return 0;
    }
}

void Mouse_Init(BootInfo *binfo)
{
    mouse_boot_info = binfo;

    uint8_t status;

    /* 보조 마우스 장치 활성화 */
    Mouse_Wait(1);
    outb(MOUSE_PORT_COMMAND, 0xA8);

    /* 인터럽트 활성화 설정 */
    Mouse_Wait(1);
    outb(MOUSE_PORT_COMMAND, 0x20);
    Mouse_Wait(0);
    status = (inb(MOUSE_PORT_DATA) | 2);
    Mouse_Wait(1);
    outb(MOUSE_PORT_COMMAND, 0x60);
    Mouse_Wait(1);
    outb(MOUSE_PORT_DATA, status);

    /* 기본 설정 복원 */
    Mouse_Write(0xF6);
    Mouse_Read();

    /* IntelliMouse 확장 활성화 시도 (4바이트 패킷 = 휠 지원) */
    mouse_has_wheel = Mouse_EnableIntelliMouse();

    /* 데이터 전송 시작 */
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
        uint8_t packet_size = mouse_has_wheel ? 4 : 3;
        mouse_packet[mouse_cycle++] = inb(MOUSE_PORT_DATA);

        if (mouse_cycle == packet_size)
        {
            mouse_cycle = 0;

            current_mouse_state.buttons = mouse_packet[0] & 0x07;

            int32_t x_offset = (int32_t)mouse_packet[1];
            int32_t y_offset = (int32_t)mouse_packet[2];

            if (mouse_packet[0] & 0x10) x_offset |= 0xFFFFFF00;
            if (mouse_packet[0] & 0x20) y_offset |= 0xFFFFFF00;

            /* 4번째 바이트: Z축(스크롤 휠) — 부호 있는 값 */
            if (mouse_has_wheel)
            {
                int8_t z = (int8_t)mouse_packet[3];
                if (z != 0)
                {
                    /*
                     * PS/2에서 z > 0 = 아래 스크롤, z < 0 = 위 스크롤
                     * 부호를 반전하여 양수 = 위, 음수 = 아래로 통일
                     */
                    scroll_delta = -z;
                    current_mouse_state.scroll = -z;
                }
            }

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

/*
 * Mouse_GetScroll: 스크롤 델타를 반환하고 내부 값을 0으로 리셋합니다.
 * 메인 루프 또는 인터럽트 핸들러에서 호출하여 스크롤 이벤트를 소비합니다.
 */
int8_t Mouse_GetScroll(void)
{
    int8_t delta = scroll_delta;
    scroll_delta = 0;
    current_mouse_state.scroll = 0;
    return delta;
}
