#include "keyboard.h"
#include "task.h"
#include "spinlock.h"

/* 키보드 스핀락 */
static spinlock_t kbd_lock = {0};

/* 키보드 원형 버퍼 */
static char kbd_buffer[KEYBOARD_BUFFER_SIZE];
static int kbd_head = 0;
static int kbd_tail = 0;

/* Scan Code Set 1 - Shift 미눌림 상태 */
static char ScanCodeTable[] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\',
    'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' '
};

/* Scan Code Set 1 - Shift 눌림 상태 */
static char ScanCodeTableShift[] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0, '|',
    'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' '
};

/* Shift 키 상태 변수 */
static bool shift_pressed = false;

void Keyboard_Handler() {
    uint8_t scancode = inb(0x60);

    /* Shift 키 눌림 및 떨어짐 감지 */
    if (scancode == 0x2A || scancode == 0x36) {
        shift_pressed = true;
        return;
    }
    if (scancode == 0xAA || scancode == 0xB6) {
        shift_pressed = false;
        return;
    }

    /* 키를 눌렀을 때만 (0x80 보다 작을 때) 처리 */
    if (scancode < 0x80) {
        char c = shift_pressed ? ScanCodeTableShift[scancode] : ScanCodeTable[scancode];
        if (c != 0) {
            uint64_t flags = spinlock_lock_irqsave(&kbd_lock);
            /* 버퍼에 공간이 있으면 저장 */
            int next = (kbd_head + 1) % KEYBOARD_BUFFER_SIZE;
            if (next != kbd_tail) {
                kbd_buffer[kbd_head] = c;
                kbd_head = next;
            }
            spinlock_unlock_irqrestore(&kbd_lock, flags);

            /* 에코 (화면에 즉시 출력) */
            char str[2] = {c, '\0'};
            kPrintString(boot_info_global, str, 0x00FFFFFF);
        }
    }
}

char Keyboard_GetChar() {
    /* 버퍼가 비어있으면 데이터가 들어올 때까지 양보 */
    while (1) {
        uint64_t flags = spinlock_lock_irqsave(&kbd_lock);
        if (kbd_head != kbd_tail) {
            char c = kbd_buffer[kbd_tail];
            kbd_tail = (kbd_tail + 1) % KEYBOARD_BUFFER_SIZE;
            spinlock_unlock_irqrestore(&kbd_lock, flags);
            return c;
        }
        spinlock_unlock_irqrestore(&kbd_lock, flags);
        Yield();
    }
}
