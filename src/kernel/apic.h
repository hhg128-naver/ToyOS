#ifndef APIC_H
#define APIC_H

#include "kernel.h"
#include <stdbool.h>

/* ===== Local APIC MMIO 베이스 주소 ===== */
#define APIC_BASE_ADDR          0xFEE00000ULL

/* ===== Local APIC 레지스터 오프셋 ===== */
#define APIC_ID_REG             0x020  /* Local APIC ID */
#define APIC_VERSION_REG        0x030  /* Local APIC 버전 */
#define APIC_TPR_REG            0x080  /* Task Priority Register */
#define APIC_EOI_REG            0x0B0  /* End of Interrupt */
#define APIC_SVR_REG            0x0F0  /* Spurious Interrupt Vector Register */

/* ===== APIC Timer 관련 레지스터 ===== */
#define APIC_LVT_TIMER_REG     0x320  /* LVT Timer Register */
#define APIC_TIMER_INIT_COUNT  0x380  /* Timer Initial Count */
#define APIC_TIMER_CUR_COUNT   0x390  /* Timer Current Count */
#define APIC_TIMER_DIV_REG     0x3E0  /* Timer Divide Configuration */

/* ===== LVT Timer 모드 비트 ===== */
#define APIC_TIMER_PERIODIC    (1 << 17)  /* Periodic 모드 */
#define APIC_TIMER_MASKED      (1 << 16)  /* 인터럽트 마스킹 */

/* ===== SVR 비트 ===== */
#define APIC_SVR_ENABLE        (1 << 8)   /* APIC Software Enable */

/* ===== APIC Timer 인터럽트 벡터 ===== */
#define APIC_TIMER_VECTOR      48

/* ===== MSR 주소 ===== */
#define IA32_APIC_BASE_MSR     0x1B
#define IA32_APIC_BASE_ENABLE  (1 << 11)  /* Global APIC Enable */

/* ===== IPI (Interprocessor Interrupt) — ICR 레지스터 ===== */
#define APIC_ICR_LOW_REG        0x300  /* Interrupt Command Register (하위 32비트) */
#define APIC_ICR_HIGH_REG       0x310  /* Interrupt Command Register (상위 32비트) */

/* ICR 딜리버리 모드 (비트 [10:8]) */
#define APIC_IPI_FIXED          (0 << 8)   /* Fixed: 지정 벡터로 인터럽트 전달 */
#define APIC_IPI_NMI            (4 << 8)   /* NMI: Non-Maskable Interrupt */
#define APIC_IPI_INIT           (5 << 8)   /* INIT: AP 리셋 신호 */
#define APIC_IPI_STARTUP        (6 << 8)   /* SIPI: Start-Up IPI */

/* ICR 레벨 (비트 14) */
#define APIC_IPI_ASSERT         (1 << 14)  /* Level Assert */
#define APIC_IPI_DEASSERT       (0 << 14)  /* Level De-assert */

/* ICR 트리거 모드 (비트 15) */
#define APIC_IPI_EDGE           (0 << 15)  /* Edge Triggered */
#define APIC_IPI_LEVEL          (1 << 15)  /* Level Triggered */

/* ICR 딜리버리 상태 (비트 12, 읽기 전용) */
#define APIC_ICR_PENDING        (1 << 12)  /* 전송 대기 중 */

/* APIC 타이머의 분주기 설정 값 (Divide Configuration Register에 쓰이는 인코딩)
bits:  [31:4]  [3]  [2]  [1]  [0]
		예약    D2   0    D1   D0
						  ↑
					 항상 0 (예약)
분주값 = 2^(D2:D1:D0 + 1)   (단, 111일 때는 예외로 1)
*/
#define APIC_TIMER_DIV_1    0x0B
#define APIC_TIMER_DIV_2    0x00
#define APIC_TIMER_DIV_4    0x01
#define APIC_TIMER_DIV_8    0x02
#define APIC_TIMER_DIV_16   0x03
#define APIC_TIMER_DIV_32   0x08
#define APIC_TIMER_DIV_64   0x09
#define APIC_TIMER_DIV_128  0x0A

#define SYSTEM_CONTROL_PORT_B  0x61		// Port 0x61 (System Control Port B)이 Channel 2를 제어합니다.
#define PIT_CHANNEL2_PORT      0x42
#define PIT_COMMAND_PORT       0x43
#define PIT_CMD_CH2_ONESHOT    0xB0      /* Channel 2, Mode 0 (One-Shot), LSB/MSB, Binary */

#define PIT_CALIBRATION_FREQ   100       /* 100Hz = 10ms 주기 */
#define PIT_BASE_FREQ          1193182
#define PIT_CAL_COUNT          (PIT_BASE_FREQ / PIT_CALIBRATION_FREQ)

/* ===== 함수 선언 ===== */

/**
 * APIC_Init: Local APIC를 활성화하고 기본 설정을 수행합니다.
 * - MSR 0x1B를 통해 APIC를 전역적으로 활성화
 * - Spurious Interrupt Vector Register 설정
 * - Task Priority Register를 0으로 설정 (모든 인터럽트 수용)
 */
void APIC_Init(void);

/**
 * APIC_Timer_Init: 지정된 주파수(Hz)로 APIC 타이머를 시작합니다.
 * - PIT를 이용한 캘리브레이션으로 버스 클럭 주파수를 자동 측정
 * - Periodic 모드로 동작하며 벡터 48번으로 인터럽트 발생
 * @param frequency_hz: 초당 인터럽트 발생 횟수 (예: 100 -> 100Hz, 10ms 주기)
 */
void APIC_Timer_Init(uint32_t frequency_hz);

/**
 * APIC_SendEOI: Local APIC에 End of Interrupt 신호를 전송합니다.
 * APIC Timer 인터럽트 처리 후 반드시 호출해야 합니다.
 */
void APIC_SendEOI(void);

/**
 * APIC_SendIPI: 지정한 대상 LAPIC에 IPI를 전송합니다.
 * @param dest_lapic_id: 대상 CPU의 Local APIC ID
 * @param vector:        인터럽트 벡터 (SIPI의 경우 트램펄린 페이지 번호)
 * @param delivery_mode: APIC_IPI_INIT 또는 APIC_IPI_STARTUP 등
 */
void APIC_SendIPI(uint8_t dest_lapic_id, uint8_t vector, uint32_t delivery_mode);

/**
 * APIC_Init_AP: AP용 Local APIC 초기화.
 * BSP의 APIC_Init()에서 이미 MMIO 매핑이 완료된 상태를 가정하며,
 * VMM_MapPage 및 I/O APIC 검사를 생략합니다.
 */
void APIC_Init_AP(void);

void IOAPIC_Write(uint32_t reg, uint32_t value);
uint32_t IOAPIC_Read(uint32_t reg);
void IOAPIC_SetRedirectionEntry(uint8_t pin, uint8_t vector, uint8_t dest_lapic_id, uint16_t flags, bool mask);
void IOAPIC_Init(void);

/* APIC 레지스터 읽기/쓰기 */
uint32_t APIC_Read(uint32_t offset);
void APIC_Write(uint32_t offset, uint32_t value);

/* 어셈블리(asm_utils.asm)에서 정의된 MSR 함수 */
extern uint64_t ReadMSR(uint32_t msr);
extern void WriteMSR(uint32_t msr, uint64_t value);

/* Interrupt Source Override Flags 가독성을 위한 상수 */
#define ISO_FLAGS_POLARITY_MASK         0x03
#define ISO_FLAGS_POLARITY_ACTIVE_LOW   0x02  /* 0x02 비트가 세팅되면 Active Low */
#define ISO_FLAGS_TRIGGER_MASK          0x0C
#define ISO_FLAGS_TRIGGER_LEVEL         0x08  /* 0x08 비트가 세팅되면 Level Triggered */

#endif
