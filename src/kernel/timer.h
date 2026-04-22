#ifndef TIMER_H
#define TIMER_H

#include "kernel.h"

#define PIT_COMMAND_PORT 0x43
#define PIT_CHANNEL0_PORT 0x40

/* PIT 설정: Channel 0, LSB/MSB, Square Wave Mode, Binary */
#define PIT_MODE_SQUARE_WAVE 0x36

/* PIT의 기본 주파수: 1.193182 MHz */
#define PIT_BASE_FREQUENCY 1193182

/**
 * PIT_Init: 지정된 주파수(Hz)로 타이머 인터럽트 주기를 설정합니다.
 * @param frequency: 초당 인터럽트 발생 횟수 (예: 100 -> 100Hz, 10ms 주기)
 */
void PIT_Init(uint32_t frequency);

#endif
