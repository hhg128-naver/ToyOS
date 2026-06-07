#include "apic.h"
#include "vmm.h"
#include <stdio.h>

/* 어셈블리(asm_utils.asm)에서 정의됨 */
extern void outb(uint16_t port, uint8_t data);
extern uint8_t inb(uint16_t port);

/*
 * APIC_Read: Local APIC MMIO 레지스터를 읽습니다.
 * @param offset: 레지스터 오프셋 (예: APIC_ID_REG = 0x020)
 */
uint32_t APIC_Read(uint32_t offset)
{
    volatile uint32_t *reg = (volatile uint32_t *)(APIC_BASE_ADDR + offset);
    return *reg;
}

/*
 * APIC_Write: Local APIC MMIO 레지스터에 값을 기록합니다.
 * @param offset: 레지스터 오프셋
 * @param value: 기록할 32비트 값
 */
void APIC_Write(uint32_t offset, uint32_t value)
{
    volatile uint32_t *reg = (volatile uint32_t *)(APIC_BASE_ADDR + offset);
    *reg = value;
}

/*
 * APIC_SendEOI: End of Interrupt 신호를 Local APIC에 전송합니다.
 */
void APIC_SendEOI(void)
{
    APIC_Write(APIC_EOI_REG, 0);
}

/*
 * APIC_Init: Local APIC를 활성화하고 기본 레지스터를 설정합니다.
 *
 * 순서:
 * 1. MSR IA32_APIC_BASE를 읽어 현재 APIC 베이스 주소 확인
 * 2. Global Enable 비트 설정
 * 3. APIC MMIO 영역에 대한 페이지 매핑 (캐시 비활성화)
 * 4. Spurious Interrupt Vector Register (SVR) 설정
 * 5. Task Priority Register (TPR) 초기화
 */
void APIC_Init(void)
{
    /* 1. MSR에서 APIC 베이스 주소 및 상태 읽기 */
    uint64_t apic_base_msr = ReadMSR(IA32_APIC_BASE_MSR);
    uint64_t apic_phys_base = apic_base_msr & 0xFFFFF000ULL;

    printf("APIC Base MSR: %p (Phys Base: %p)\n",
           (void *)apic_base_msr, (void *)apic_phys_base);

    /* 2. Global Enable 비트가 꺼져 있으면 활성화 */
    if (!(apic_base_msr & IA32_APIC_BASE_ENABLE))
    {
        apic_base_msr |= IA32_APIC_BASE_ENABLE;
        WriteMSR(IA32_APIC_BASE_MSR, apic_base_msr);
        printf("APIC Global Enable bit set.\n");
    }

    /*
     * 3. APIC MMIO 영역을 페이지 테이블에 매핑 (캐시 비활성화)
     *    VMM_Init에서 0~4GB를 identity mapping하지만,
     *    APIC 레지스터는 캐시되면 안 되므로 PCD+PWT 플래그를 추가합니다.
     */
    VMM_MapPage(
        (void *)APIC_BASE_ADDR,
        (void *)APIC_BASE_ADDR,
        PAGE_PRESENT | PAGE_WRITABLE | PAGE_CACHE_DISABLE | PAGE_WRITE_THROUGH
    );

    /* 4. Spurious Interrupt Vector Register 설정 */
    /*    벡터 = 0xFF (Spurious), Software Enable = 1 */
    APIC_Write(APIC_SVR_REG, APIC_SVR_ENABLE | 0xFF);

    /* 5. Task Priority Register를 0으로 설정 (모든 인터럽트 수용) */
    APIC_Write(APIC_TPR_REG, 0);

    /* 초기화 결과 출력 */
    uint32_t apic_id = APIC_Read(APIC_ID_REG) >> 24;
    uint32_t apic_ver = APIC_Read(APIC_VERSION_REG) & 0xFF;
    printf("APIC Initialized: ID=%u, Version=0x%x\n", apic_id, apic_ver);

    /* I/O APIC 감지 및 상태 확인 */
    #define IOAPIC_BASE_ADDR        0xFEC00000ULL
    #define IOAPIC_REG_SEL          0x00
    #define IOAPIC_REG_WIN          0x10
    #define IOAPIC_ID_INDEX         0x00
    #define IOAPIC_VER_INDEX        0x01

    VMM_MapPage(
        (void *)IOAPIC_BASE_ADDR,
        (void *)IOAPIC_BASE_ADDR,
        PAGE_PRESENT | PAGE_WRITABLE | PAGE_CACHE_DISABLE | PAGE_WRITE_THROUGH
    );

    volatile uint32_t *ioapic_sel = (volatile uint32_t *)(IOAPIC_BASE_ADDR + IOAPIC_REG_SEL);
    volatile uint32_t *ioapic_win = (volatile uint32_t *)(IOAPIC_BASE_ADDR + IOAPIC_REG_WIN);

    *ioapic_sel = IOAPIC_VER_INDEX;
    uint32_t ioapic_ver = *ioapic_win;

    *ioapic_sel = IOAPIC_ID_INDEX;
    uint32_t ioapic_id = *ioapic_win;

    if (ioapic_ver == 0xFFFFFFFF || ioapic_ver == 0x00000000)
    {
        printf("I/O APIC: NOT Active or Not Found (Read: 0x%08X)\n", ioapic_ver);
    }
    else
    {
        uint8_t io_version = ioapic_ver & 0xFF;
        uint8_t io_max_redir = (ioapic_ver >> 16) & 0xFF;
        uint8_t io_id = (ioapic_id >> 24) & 0x0F;
        printf("I/O APIC: Active (ID=%u, Version=0x%02X, Max Redirection Entries=%u)\n",
               io_id, io_version, io_max_redir + 1);
    }
}

/*
 * APIC_Timer_Init: PIT를 이용한 캘리브레이션 후 APIC 타이머를 시작합니다.
 *
 * 캘리브레이션 과정:
 * 1. APIC Timer Divide Register를 16으로 설정
 * 2. PIT Channel 2를 10ms one-shot으로 프로그래밍
 * 3. APIC Timer를 0xFFFFFFFF로 시작하고 PIT 완료까지 대기
 * 4. 경과 틱으로 버스 주파수를 역산
 * 5. 원하는 주파수에 맞는 Initial Count 설정
 * 6. Periodic 모드 + 벡터 48로 타이머 가동
 *
 * @param frequency_hz: 원하는 타이머 주파수 (Hz)
 */
void APIC_Timer_Init(uint32_t frequency_hz)
{
    /*
     * Timer Divide Configuration Register 값:
     * 0x03 = divide by 16
     * 다른 값: 0x00(÷1), 0x01(÷4), 0x02(÷8), 0x03(÷16),
     *          0x08(÷32), 0x09(÷64), 0x0A(÷128), 0x0B(÷1)
     */
    APIC_Write(APIC_TIMER_DIV_REG, 0x03);

    /*
     * === PIT 캘리브레이션 ===
     * PIT Channel 2를 사용하여 정확한 10ms를 측정합니다.
     * PIT 기본 주파수: 1,193,182 Hz
     * 10ms에 해당하는 카운트: 1193182 / 100 = 11931
     */
    #define PIT_CALIBRATION_FREQ   100       /* 100Hz = 10ms 주기 */
    #define PIT_BASE_FREQ          1193182
    #define PIT_CAL_COUNT          (PIT_BASE_FREQ / PIT_CALIBRATION_FREQ)

    /* PIT Channel 2 설정: Mode 0 (One-shot), LSB/MSB */
    outb(0x61, (inb(0x61) & 0xFD) | 0x01);  /* Gate 활성화, Speaker 비활성화 */
    outb(0x43, 0xB0);                        /* Channel 2, Mode 0, LSB/MSB */
    outb(0x42, (uint8_t)(PIT_CAL_COUNT & 0xFF));        /* LSB */
    outb(0x42, (uint8_t)((PIT_CAL_COUNT >> 8) & 0xFF)); /* MSB */

    /* APIC Timer를 one-shot 모드로 시작 (최대 카운트) */
    APIC_Write(APIC_LVT_TIMER_REG, APIC_TIMER_MASKED | APIC_TIMER_VECTOR);
    APIC_Write(APIC_TIMER_INIT_COUNT, 0xFFFFFFFF);

    /* PIT Channel 2 출력이 High가 될 때까지 대기 (카운트 완료) */
    while (!(inb(0x61) & 0x20))
    {
        /* busy wait */
    }

    /* APIC Timer 정지 */
    APIC_Write(APIC_LVT_TIMER_REG, APIC_TIMER_MASKED);

    /* 경과한 APIC 틱 수 계산 */
    uint32_t elapsed = 0xFFFFFFFF - APIC_Read(APIC_TIMER_CUR_COUNT);

    /*
     * elapsed 틱은 10ms(= 1/PIT_CALIBRATION_FREQ 초) 동안 발생한 수이므로
     * 버스 주파수 = elapsed * PIT_CALIBRATION_FREQ * divider
     * APIC 타이머 틱/초 = elapsed * PIT_CALIBRATION_FREQ (divider는 이미 적용됨)
     */
    uint32_t ticks_per_sec = elapsed * PIT_CALIBRATION_FREQ;

    printf("APIC Timer Calibrated: %u ticks in 10ms, Ticks/sec = %u\n",
           elapsed, ticks_per_sec);

    /* 원하는 주파수에 맞는 Initial Count 계산 */
    uint32_t init_count = ticks_per_sec / frequency_hz;
    if (init_count == 0)
        init_count = 1;

    /* LVT Timer Register 설정: Periodic 모드 + 벡터 48 */
    APIC_Write(APIC_LVT_TIMER_REG, APIC_TIMER_PERIODIC | APIC_TIMER_VECTOR);

    /* Initial Count 설정 → 타이머 시작 */
    APIC_Write(APIC_TIMER_INIT_COUNT, init_count);

    printf("APIC Timer Started: %u Hz (Initial Count: %u)\n",
           frequency_hz, init_count);
}

/*
 * APIC_SendIPI: ICR(Interrupt Command Register)을 통해 대상 LAPIC에 IPI를 전송합니다.
 *
 * ICR 상위 레지스터(0x310)에 대상 LAPIC ID를 먼저 쓰고,
 * 그 다음 하위 레지스터(0x300)에 명령을 쓰면 IPI가 발사됩니다.
 *
 * @param dest_lapic_id: 대상 CPU의 Local APIC ID
 * @param vector:        인터럽트 벡터 (예: SIPI = 트램펄린 페이지 번호)
 * @param delivery_mode: APIC_IPI_INIT 또는 APIC_IPI_STARTUP
 */
void APIC_SendIPI(uint8_t dest_lapic_id, uint8_t vector, uint32_t delivery_mode)
{
    /* 1. ICR 상위 레지스터에 대상 LAPIC ID 설정 */
    APIC_Write(APIC_ICR_HIGH_REG, (uint32_t)dest_lapic_id << 24);

    /* 2. ICR 하위 레지스터 작성 → IPI 발사 */
    uint32_t icr_low = (uint32_t)vector | delivery_mode | APIC_IPI_ASSERT | APIC_IPI_EDGE;
    APIC_Write(APIC_ICR_LOW_REG, icr_low);

    /* 3. 딜리버리 완료 대기 (Delivery Status 비트 클리어 될 때까지) */
    while (APIC_Read(APIC_ICR_LOW_REG) & APIC_ICR_PENDING)
    {
        __asm__ volatile("pause");
    }
}

/*
 * APIC_Init_AP: AP용 Local APIC 초기화.
 *
 * BSP의 APIC_Init()에서 MMIO 매핑이 이미 완료되었으므로
 * VMM_MapPage 및 I/O APIC 검사를 생략하고 SVR/TPR만 설정합니다.
 */
void APIC_Init_AP(void)
{
    /* Global Enable 비트 확인 및 필요 시 활성화 */
    uint64_t apic_base_msr = ReadMSR(IA32_APIC_BASE_MSR);
    if (!(apic_base_msr & IA32_APIC_BASE_ENABLE))
    {
        apic_base_msr |= IA32_APIC_BASE_ENABLE;
        WriteMSR(IA32_APIC_BASE_MSR, apic_base_msr);
    }

    /* Spurious Interrupt Vector Register: APIC 소프트웨어 활성화 + Spurious 벡터 0xFF */
    APIC_Write(APIC_SVR_REG, APIC_SVR_ENABLE | 0xFF);

    /* Task Priority Register: 0 (모든 인터럽트 수용) */
    APIC_Write(APIC_TPR_REG, 0);
}
