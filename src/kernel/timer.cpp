#include "timer.h"
#include <stdio.h>




void PIT_Init(uint32_t frequency) {
    /* 1. 분주기(Divisor) 계산: 1,193,182 / target_frequency */
    uint32_t divisor = PIT_BASE_FREQUENCY / frequency;

    /* 2. PIT 커맨드 전송: Channel 0, Square Wave Mode */
    outb(PIT_COMMAND_PORT, PIT_MODE_SQUARE_WAVE);

    /* 3. LSB (하위 8비트) 전송 */
    outb(PIT_CHANNEL0_PORT, (uint8_t)(divisor & 0xFF));

    /* 4. MSB (상위 8비트) 전송 */
    outb(PIT_CHANNEL0_PORT, (uint8_t)((divisor >> 8) & 0xFF));

    kPrintf("PIT Initialized: %u Hz (Divisor: %u)\n", frequency, divisor);
}
