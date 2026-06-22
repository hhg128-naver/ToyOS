#include "pic.h"




#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1

void PIC_Init()
{
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

/*
 * PIC_MaskIRQ: 특정 IRQ 라인을 마스킹(비활성화)합니다.
 * @param irq: 마스킹할 IRQ 번호 (0~15)
 */
void PIC_MaskIRQ(uint8_t irq)
{
	uint16_t port;
	if (irq < 8)
	{
		port = PIC1_DATA;
	}
	else
	{
		port = PIC2_DATA;
		irq -= 8;
	}
	uint8_t mask = inb(port) | (1 << irq);
	outb(port, mask);
}

/*
 * PIC_UnmaskIRQ: 특정 IRQ 라인을 언마스킹(활성화)합니다.
 * @param irq: 언마스킹할 IRQ 번호 (0~15)
 */
void PIC_UnmaskIRQ(uint8_t irq)
{
	uint16_t port;
	if (irq < 8) 
	{
		port = PIC1_DATA;
	}
	else 
	{
		port = PIC2_DATA;
		irq -= 8;
	}
	uint8_t mask = inb(port) & ~(1 << irq);
	outb(port, mask);
}

/*
 * PIC_Disable: 8259A PIC의 모든 인터럽트 핀을 마스킹하여 비활성화합니다.
 */
void PIC_Disable(void)
{
	outb(PIC1_DATA, 0xFF);
	outb(PIC2_DATA, 0xFF);
}

