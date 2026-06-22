#include "console.h"
#include "font.h"
#include <string.h>
#include <stdio.h>

/*
 * ===== 스크롤백 버퍼 설정 =====
 *
 * 콘솔 출력을 Ring Buffer에 저장하여 마우스 휠로 이전 출력을 다시 볼 수 있게 합니다.
 * SCROLLBACK_LINES: 보관할 최대 줄 수 (이 이상이면 가장 오래된 줄부터 덮어씀)
 * MAX_COLS: 한 줄에 들어갈 수 있는 최대 문자 수 (8px 폰트 기준)
 */
#define SCROLLBACK_LINES  500
#define MAX_COLS          160  /* 1280 / 8 = 160 (최대 해상도 기준) */

/*
 * 텍스트 버퍼: 각 칸에 문자와 색상을 저장합니다.
 * Ring Buffer 방식으로 동작하여 SCROLLBACK_LINES를 넘어가면
 * 가장 오래된 줄부터 덮어씁니다.
 */
static char    text_buffer[SCROLLBACK_LINES][MAX_COLS];
static uint32_t color_buffer[SCROLLBACK_LINES][MAX_COLS];

/*
 * Ring Buffer 상태:
 * - write_line: 다음에 기록할 줄 번호 (0 ~ SCROLLBACK_LINES-1, 순환)
 * - total_lines: 지금까지 기록된 총 줄 수 (SCROLLBACK_LINES를 넘을 수 있음)
 * - write_col: 현재 줄에서 다음에 기록할 열 번호
 */
static int write_line = 0;
static int total_lines = 0;
static int write_col = 0;

/*
 * 스크롤백 뷰 상태:
 * - view_offset: 현재 보고 있는 위치가 최신 줄에서 몇 줄 위인지 (0 = 최신)
 * - is_scrolled_back: 스크롤백 상태인지 여부
 */
static int view_offset = 0;
static int is_scrolled_back = 0;

/*
 * 그래픽 콘솔 상태
 */
static int cursor_x = 0;
static int cursor_y = 0;
BootInfo *boot_info_global = nullptr;
static uint32_t current_bg_color = 0x00000033;

/* 화면에 표시할 수 있는 줄 수 */
static int screen_rows = 0;
/* 실제 사용할 최대 열 수 */
static int actual_max_cols = 0;

void Console_Init(BootInfo *binfo)
{
    boot_info_global = binfo;

    /* 화면 크기 기반으로 행/열 수 계산 */
    screen_rows = binfo->vertical_resolution / 16;
    actual_max_cols = binfo->horizontal_resolution / 8;
    if (actual_max_cols > MAX_COLS) actual_max_cols = MAX_COLS;

    /* 텍스트 버퍼 초기화 */
    memset(text_buffer, 0, sizeof(text_buffer));
    memset(color_buffer, 0, sizeof(color_buffer));

    ClearScreen(binfo, 0x00000033);
}

void kPrintf(const char *str, ...)
{
	va_list ap;
	char s[1024];

	va_start(ap, str);
	vsprintf(s, str, ap);
	va_end(ap);

    kPrintString(boot_info_global, s, 0x00FFFFFF);
}

void ClearScreen(BootInfo *binfo, uint32_t color)
{
    uint32_t *vidptr = binfo->framebuffer;
    for (uint64_t i = 0; i < binfo->screen_size; i++)
    {
        vidptr[i] = color;
    }
    cursor_x = 0;
    cursor_y = 0;
    current_bg_color = color;
}

void kPutChar(BootInfo *binfo, int x, int y, char c, uint32_t color, uint32_t bg_color)
{
    unsigned char *font_ptr = &EnglishFont[(unsigned char)c * 16];
    uint32_t *vidptr = binfo->framebuffer;

    for (int dy = 0; dy < 16; dy++)
    {
        for (int dx = 0; dx < 8; dx++)
        {
            if ((font_ptr[dy] << dx) & 0x80)
            {
                vidptr[(y + dy) * binfo->horizontal_resolution + (x + dx)] = color;
            }
            else
            {
                vidptr[(y + dy) * binfo->horizontal_resolution + (x + dx)] = bg_color;
            }
        }
    }
}

/*
 * ScrollBuffer_NewLine: 텍스트 버퍼에서 새 줄로 이동합니다.
 * 현재 줄을 확정하고 다음 줄을 초기화합니다.
 */
static void ScrollBuffer_NewLine(void)
{
    write_col = 0;
    write_line = (write_line + 1) % SCROLLBACK_LINES;
    total_lines++;

    /* 새 줄 초기화 */
    memset(text_buffer[write_line], 0, MAX_COLS);
    memset(color_buffer[write_line], 0, MAX_COLS * sizeof(uint32_t));
}

/*
 * ScrollBuffer_PutChar: 텍스트 버퍼에 한 문자를 기록합니다.
 *
 * @param c: 기록할 문자
 * @param color: 문자의 색상
 */
static void ScrollBuffer_PutChar(char c, uint32_t color)
{
    if (c == '\n')
    {
        ScrollBuffer_NewLine();
        return;
    }

    if (c == '\b')
    {
        if (write_col > 0)
        {
            write_col--;
            text_buffer[write_line][write_col] = ' ';
            color_buffer[write_line][write_col] = current_bg_color;
        }
        return;
    }

    if (write_col < actual_max_cols)
    {
        text_buffer[write_line][write_col] = c;
        color_buffer[write_line][write_col] = color;
        write_col++;
    }

    /* 열이 꽉 차면 자동 줄바꿈 */
    if (write_col >= actual_max_cols)
    {
        ScrollBuffer_NewLine();
    }
}

/*
 * Console_RenderFromBuffer: 텍스트 버퍼의 특정 범위를 프레임버퍼에 렌더링합니다.
 * view_offset에 따라 보여줄 줄의 범위를 결정합니다.
 */
static void Console_RenderFromBuffer(void)
{
    BootInfo *binfo = boot_info_global;
    if (!binfo) return;

    /* 사용 가능한 줄 수 계산 */
    int available_lines = total_lines + 1;  /* 현재 작성 중인 줄 포함 */
    if (available_lines > SCROLLBACK_LINES) available_lines = SCROLLBACK_LINES;

    /* 화면 전체를 배경색으로 초기화 */
    uint32_t *vidptr = binfo->framebuffer;
    for (uint64_t i = 0; i < binfo->screen_size; i++)
    {
        vidptr[i] = current_bg_color;
    }

    /* 표시할 줄 수 (화면 행 수와 사용 가능 줄 수 중 작은 것) */
    int display_lines = screen_rows;
    if (display_lines > available_lines) display_lines = available_lines;

    /*
     * 표시할 범위 결정:
     * 가장 최근 줄(write_line)에서 view_offset만큼 위로 올라간 위치가 표시 범위의 끝
     * 거기서 화면 행 수만큼 올라간 위치가 표시 범위의 시작
     */
    int end_line_idx = write_line - view_offset;
    if (end_line_idx < 0) end_line_idx += SCROLLBACK_LINES;

    for (int row = 0; row < display_lines; row++)
    {
        /* 표시할 줄의 버퍼 인덱스 계산 (아래에서부터 채움) */
        int buf_idx = end_line_idx - (display_lines - 1 - row);
        if (buf_idx < 0) buf_idx += SCROLLBACK_LINES;
        buf_idx %= SCROLLBACK_LINES;

        int screen_y = row * 16;

        for (int col = 0; col < actual_max_cols; col++)
        {
            char c = text_buffer[buf_idx][col];
            if (c == 0) break;  /* 줄 끝 */

            uint32_t color = color_buffer[buf_idx][col];
            if (color == 0) color = 0x00FFFFFF;

            kPutChar(binfo, col * 8, screen_y, c, color, current_bg_color);
        }
    }
}

static void ScrollUp(BootInfo *binfo)
{
    uint32_t *vidptr = binfo->framebuffer;
    uint32_t width = binfo->horizontal_resolution;
    uint32_t height = binfo->vertical_resolution;

    uint64_t scroll_pixels = (uint64_t)(height - 16) * width;
    uint64_t offset_pixels = (uint64_t)16 * width;

    for (uint64_t i = 0; i < scroll_pixels; i++)
    {
        vidptr[i] = vidptr[i + offset_pixels];
    }

    uint64_t total_pixels = (uint64_t)height * width;
    for (uint64_t i = scroll_pixels; i < total_pixels; i++)
    {
        vidptr[i] = current_bg_color;
    }
}

void kPrintString(BootInfo *binfo, const char *str, uint32_t color)
{
    /*
     * 스크롤백 상태에서 새 출력이 발생하면 자동으로 최신 위치로 복귀합니다.
     * 이렇게 해야 새 출력이 화면에 바로 보입니다.
     */
    if (is_scrolled_back)
    {
        view_offset = 0;
        is_scrolled_back = 0;
        Console_RenderFromBuffer();
        /* 커서 위치를 화면 맨 아래로 재설정 */
        int available = total_lines + 1;
        if (available > SCROLLBACK_LINES) available = SCROLLBACK_LINES;
        int visible = available < screen_rows ? available : screen_rows;
        cursor_y = (visible - 1) * 16;
        cursor_x = write_col * 8;
    }

    for (int i = 0; str[i] != '\0'; i++)
    {
        /* 텍스트 버퍼에 기록 */
        ScrollBuffer_PutChar(str[i], color);

        if (str[i] == '\n')
        {
            cursor_x = 0;
            cursor_y += 16;
        }
        else if (str[i] == '\b')
        {
            if (cursor_x >= 8)
            {
                cursor_x -= 8;
                kPutChar(binfo, cursor_x, cursor_y, ' ', current_bg_color, current_bg_color);
            }
        }
        else
        {
            kPutChar(binfo, cursor_x, cursor_y, str[i], color, current_bg_color);
            cursor_x += 8;
        }

        if (cursor_x + 8 > (int)binfo->horizontal_resolution)
        {
            cursor_x = 0;
            cursor_y += 16;
        }

        if (cursor_y + 16 > (int)binfo->vertical_resolution)
        {
            ScrollUp(binfo);
            cursor_y -= 16;
        }
    }
}

void PrintStringLen(BootInfo *binfo, const char *str, uint32_t len, uint32_t color)
{
    /* 스크롤백 상태 복귀 */
    if (is_scrolled_back)
    {
        view_offset = 0;
        is_scrolled_back = 0;
        Console_RenderFromBuffer();
        int available = total_lines + 1;
        if (available > SCROLLBACK_LINES) available = SCROLLBACK_LINES;
        int visible = available < screen_rows ? available : screen_rows;
        cursor_y = (visible - 1) * 16;
        cursor_x = write_col * 8;
    }

    for (uint32_t i = 0; i < len; i++)
    {
        /* 텍스트 버퍼에 기록 */
        ScrollBuffer_PutChar(str[i], color);

        if (str[i] == '\n')
        {
            cursor_x = 0;
            cursor_y += 16;
        }
        else if (str[i] == '\b')
        {
            if (cursor_x >= 8)
            {
                cursor_x -= 8;
                kPutChar(binfo, cursor_x, cursor_y, ' ', current_bg_color, current_bg_color);
            }
        }
        else
        {
            kPutChar(binfo, cursor_x, cursor_y, str[i], color, current_bg_color);
            cursor_x += 8;
        }

        if (cursor_x + 8 > (int)binfo->horizontal_resolution)
        {
            cursor_x = 0;
            cursor_y += 16;
        }

        if (cursor_y + 16 > (int)binfo->vertical_resolution)
        {
            ScrollUp(binfo);
            cursor_y -= 16;
        }
    }
}

/*
 * Console_HandleScroll: 마우스 휠 이벤트를 처리하여 콘솔 스크롤을 수행합니다.
 *
 * 양수 delta = 위로 스크롤 (이전 내용 보기)
 * 음수 delta = 아래로 스크롤 (최신 내용으로 복귀)
 *
 * 스크롤 시 텍스트 버퍼에서 해당 범위의 내용을 프레임버퍼에 다시 렌더링합니다.
 * 매 휠 클릭당 3줄씩 이동합니다.
 *
 * @param delta: 스크롤 방향 및 크기
 */
void Console_HandleScroll(int8_t delta)
{
    if (!boot_info_global) return;

    int scroll_step = 3;  /* 한 번의 휠 클릭당 이동할 줄 수 */

    /* 사용 가능한 줄 수 (화면에 안 보이는 부분) */
    int available_lines = total_lines + 1;
    if (available_lines > SCROLLBACK_LINES) available_lines = SCROLLBACK_LINES;

    int max_offset = available_lines - screen_rows;
    if (max_offset < 0) max_offset = 0;

    if (delta > 0)
    {
        /* 위로 스크롤 (이전 내용 보기) */
        view_offset += scroll_step;
        if (view_offset > max_offset) view_offset = max_offset;
        is_scrolled_back = (view_offset > 0) ? 1 : 0;
    }
    else if (delta < 0)
    {
        /* 아래로 스크롤 (최신 내용으로) */
        view_offset -= scroll_step;
        if (view_offset < 0) view_offset = 0;
        is_scrolled_back = (view_offset > 0) ? 1 : 0;
    }

    /* 텍스트 버퍼에서 화면 다시 렌더링 */
    Console_RenderFromBuffer();
}
