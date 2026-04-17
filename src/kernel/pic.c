#include "pic.h"

/* 어셈블리(asm_utils.asm)에서 정의됨 */
extern void outb(uint16_t port, uint8_t data);
extern uint8_t inb(uint16_t port);

#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1

void PIC_Init() {
    /* ICW1: 초기화 시작 (Edge triggered, Cascade 모드) */
    outb(PIC1_COMMAND, 0x11);
    outb(PIC2_COMMAND, 0x11);

    /* ICW2: IRQ 매핑 주소(Offset) 설정 */
    outb(PIC1_DATA, 0x20); // Master: IRQ 0~7 -> 0x20~0x27
    outb(PIC2_DATA, 0x28); // Slave:  IRQ 8~15 -> 0x28~0x2F

    /* ICW3: Master/Slave 연결 설정 */
    outb(PIC1_DATA, 0x04); // Master: 2번 핀에 Slave 연결됨
    outb(PIC2_DATA, 0x02); // Slave:  2번 핀을 통해 Master에 연결됨

    /* ICW4: 추가 옵션 (8086 모드) */
    outb(PIC1_DATA, 0x01);
    outb(PIC2_DATA, 0x01);

    /* 모든 인터럽트 마스크 해제 (일단 모두 수용) */
    outb(PIC1_DATA, 0x00);
    outb(PIC2_DATA, 0x00);
}
