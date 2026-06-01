#ifndef APIC_H
#define APIC_H

#include "kernel.h"

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

/* APIC 레지스터 읽기/쓰기 */
uint32_t APIC_Read(uint32_t offset);
void APIC_Write(uint32_t offset, uint32_t value);

/* 어셈블리(asm_utils.asm)에서 정의된 MSR 함수 */
extern uint64_t ReadMSR(uint32_t msr);
extern void WriteMSR(uint32_t msr, uint64_t value);

#endif
