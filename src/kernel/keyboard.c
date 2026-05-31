#include "keyboard.h"
#include "task.h"

extern uint8_t inb(uint16_t port);
extern BootInfo *boot_info_global;

/* 키보드 원형 버퍼 */
static char kbd_buffer[KEYBOARD_BUFFER_SIZE];
static int kbd_head = 0;
static int kbd_tail = 0;

/* Scan Code Set 1 (일부) */
static char ScanCodeTable[] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\',
    'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' '
};

void Keyboard_Handler() {
    uint8_t scancode = inb(0x60);

    /* 키를 눌렀을 때만 (0x80 보다 작을 때) 처리 */
    if (scancode < 0x80) {
        char c = ScanCodeTable[scancode];
        if (c != 0) {
            /* 버퍼에 공간이 있으면 저장 */
            int next = (kbd_head + 1) % KEYBOARD_BUFFER_SIZE;
            if (next != kbd_tail) {
                kbd_buffer[kbd_head] = c;
                kbd_head = next;
            }

            /* 에코 (화면에 즉시 출력) */
            char str[2] = {c, '\0'};
            kPrintString(boot_info_global, str, 0x00FFFFFF);
        }
    }
}

char Keyboard_GetChar() {
    /* 버퍼가 비어있으면 데이터가 들어올 때까지 양보 */
    while (kbd_head == kbd_tail) {
        Yield();
    }

    char c = kbd_buffer[kbd_tail];
    kbd_tail = (kbd_tail + 1) % KEYBOARD_BUFFER_SIZE;
    return c;
}
