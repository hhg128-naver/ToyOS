#include "apic.h"
#include "vmm.h"
#include "acpi.h"
#include <stdio.h>
#include <stdbool.h>

#define IOAPIC_BASE_ADDR        0xFEC00000ULL
#define IOAPIC_REG_SEL          0x00
#define IOAPIC_REG_WIN          0x10
#define IOAPIC_ID_INDEX         0x00
#define IOAPIC_VER_INDEX        0x01


/* BSP에서 한 번 측정한 APIC 타이머 틱/초 (이후 AP가 재사용) */
static volatile uint32_t apic_calibrated_ticks_per_sec = 0;




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

void EnableGlobalLocalAPIC()
{
    uint64_t apic_base_msr = ReadMSR(IA32_APIC_BASE_MSR);
    if (!(apic_base_msr & IA32_APIC_BASE_ENABLE))
    {
        apic_base_msr |= IA32_APIC_BASE_ENABLE;
        WriteMSR(IA32_APIC_BASE_MSR, apic_base_msr);
        kPrintf("APIC: Global Enable bit set in MSR 0x1B.\n");
    }
    else
    {
        kPrintf("APIC: Global Enable bit already set in MSR 0x1B.\n");
	}
}

/* 
 * Spurious Interrupt Vector Register 설정
 * 벡터 = 0xFF (Spurious), Software Enable = 1 
 */
void EnableSoftwareLocalAPIC()
{
    APIC_Write(APIC_SVR_REG, APIC_SVR_ENABLE | 0xFF);
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
    /* MSR에서 APIC 베이스 주소 및 상태 읽기 */
    uint64_t apic_base_msr = ReadMSR(IA32_APIC_BASE_MSR);
    uint64_t apic_phys_base = apic_base_msr & 0xFFFFF000ULL;

    kPrintf("APIC Base MSR: %p (Phys Base: %p)\n", (void *)apic_base_msr, (void *)apic_phys_base);

    /* Global Enable 비트가 꺼져 있으면 활성화 */
    EnableGlobalLocalAPIC();

    /*
     *  APIC MMIO 영역을 페이지 테이블에 매핑 (캐시 비활성화)
     *    VMM_Init에서 0~4GB를 identity mapping하지만,
     *    APIC 레지스터는 캐시되면 안 되므로 PCD+PWT 플래그를 추가합니다.
     */
    VMM_MapPage(
        (void*)APIC_BASE_ADDR, 
        (void*)APIC_BASE_ADDR, 
        PAGE_PRESENT | PAGE_WRITABLE | PAGE_CACHE_DISABLE | PAGE_WRITE_THROUGH
    );

    /* Spurious Interrupt Vector Register 설정 */
    EnableSoftwareLocalAPIC();

    /* Task Priority Register를 0으로 설정 (모든 인터럽트 수용) */
    APIC_Write(APIC_TPR_REG, 0);

    /* 초기화 결과 출력 */
    uint32_t apic_id = APIC_Read(APIC_ID_REG) >> 24;
    uint32_t apic_ver = APIC_Read(APIC_VERSION_REG) & 0xFF;
    kPrintf("APIC Initialized: ID=%u, Version=0x%x\n", apic_id, apic_ver);

    /* I/O APIC 감지 및 상태 확인 */
    VMM_MapPage(
        (void*)IOAPIC_BASE_ADDR,
        (void*)IOAPIC_BASE_ADDR,
        PAGE_PRESENT | PAGE_WRITABLE | PAGE_CACHE_DISABLE | PAGE_WRITE_THROUGH
    );

	// IO APIC 레지스터에 접근하기 위한 포인터 설정
    volatile uint32_t *ioapic_sel = (volatile uint32_t *)(IOAPIC_BASE_ADDR + IOAPIC_REG_SEL);
    volatile uint32_t *ioapic_win = (volatile uint32_t *)(IOAPIC_BASE_ADDR + IOAPIC_REG_WIN);

    *ioapic_sel = IOAPIC_VER_INDEX;
    uint32_t ioapic_ver = *ioapic_win;

    *ioapic_sel = IOAPIC_ID_INDEX;
    uint32_t ioapic_id = *ioapic_win;

    if (ioapic_ver == 0xFFFFFFFF || ioapic_ver == 0x00000000)
    {
        kPrintf("I/O APIC: NOT Active or Not Found (Read: 0x%08X)\n", ioapic_ver);
    }
    else
    {
        uint8_t io_version = ioapic_ver & 0xFF;
        uint8_t io_max_redir = (ioapic_ver >> 16) & 0xFF;
        uint8_t io_id = (ioapic_id >> 24) & 0x0F;
        kPrintf("I/O APIC: Active (ID=%u, Version=0x%02X, Max Redirection Entries=%u)\n",
               io_id, io_version, io_max_redir + 1);
    }
}

void pit_channel2_start()
{
	/* BSP 경우: PIT Channel 2로 10ms 캐리브레이션 수행 */
	/* PIT Channel 2의 Gate를 활성화하고, Speaker 출력은 비활성화합니다. */

    uint8_t port_b_val = inb(SYSTEM_CONTROL_PORT_B);
    port_b_val = (port_b_val & 0xFD) | 0x01; /* Speaker 비활성화 (bit 1 = 0), Gate 활성화 (bit 0 = 1) */
    outb(SYSTEM_CONTROL_PORT_B, port_b_val);

    /* PIT Channel 2를 One-shot 모드로 프로그래밍하고 카운트 값을 설정합니다. */
    outb(PIT_COMMAND_PORT, PIT_CMD_CH2_ONESHOT);
    outb(PIT_CHANNEL2_PORT, (uint8_t)(PIT_CAL_COUNT & 0xFF));        /* LSB */
    outb(PIT_CHANNEL2_PORT, (uint8_t)((PIT_CAL_COUNT >> 8) & 0xFF)); /* MSB */
}

bool Is_pit_channel2_done(void)
{
	// bit 5: 카운터가 0에 도달하면 1
	return inb(SYSTEM_CONTROL_PORT_B) & (1 << 5);
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
    APIC_Write(APIC_TIMER_DIV_REG, APIC_TIMER_DIV_16);

    uint32_t ticks_per_sec = 0;

    if (apic_calibrated_ticks_per_sec != 0)
    {
        /*
         * AP 경우: BSP가 이미 PIT로 캐리브레이션한 값을 재사용합니다.
         * PIT Channel 2는 공유 자원이므로 AP가 동시에 접근하면 오동작합니다.
         */
        ticks_per_sec = apic_calibrated_ticks_per_sec;
        kPrintf("APIC Timer (AP, LAPIC=%u): Reusing calibrated %u ticks/sec.\n", APIC_Read(APIC_ID_REG) >> 24, ticks_per_sec);
    }
	else
	{
		/* APIC Timer를 one-shot 모드로 시작 (최대 카운트) */
		APIC_Write(APIC_LVT_TIMER_REG, APIC_TIMER_MASKED | APIC_TIMER_VECTOR);
		APIC_Write(APIC_TIMER_INIT_COUNT, 0xFFFFFFFF);

        pit_channel2_start();

		/* PIT Channel 2 출력이 High가 될 때까지 대기 (카운트 완료) */
		while (!Is_pit_channel2_done())
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
		ticks_per_sec = elapsed * PIT_CALIBRATION_FREQ;
		apic_calibrated_ticks_per_sec = ticks_per_sec; /* AP를 위해 저장 */

		kPrintf("APIC Timer Calibrated: %u ticks in 10ms, Ticks/sec = %u\n", elapsed, ticks_per_sec);
	} /* end BSP calibration block */

    /* 원하는 주파수에 맞는 Initial Count 계산 */
    uint32_t init_count = ticks_per_sec / frequency_hz;
    if (init_count == 0)
    {
        init_count = 1;
    }

    /* LVT Timer Register 설정: Periodic 모드 + 벡터 48 */
    APIC_Write(APIC_LVT_TIMER_REG, APIC_TIMER_PERIODIC | APIC_TIMER_VECTOR);

    /* Initial Count 설정 → 타이머 시작 */
    APIC_Write(APIC_TIMER_INIT_COUNT, init_count);

    kPrintf("APIC Timer Started: %u Hz (Initial Count: %u)\n", frequency_hz, init_count);
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

void IOAPIC_Write(uint32_t reg, uint32_t value)
{
    volatile uint32_t *ioapic_sel = (volatile uint32_t *)(IOAPIC_BASE_ADDR + IOAPIC_REG_SEL);
    volatile uint32_t *ioapic_win = (volatile uint32_t *)(IOAPIC_BASE_ADDR + IOAPIC_REG_WIN);
    *ioapic_sel = reg;
    *ioapic_win = value;
}

uint32_t IOAPIC_Read(uint32_t reg)
{
    volatile uint32_t *ioapic_sel = (volatile uint32_t *)(IOAPIC_BASE_ADDR + IOAPIC_REG_SEL);
    volatile uint32_t *ioapic_win = (volatile uint32_t *)(IOAPIC_BASE_ADDR + IOAPIC_REG_WIN);
    *ioapic_sel = reg;
    return *ioapic_win;
}

void IOAPIC_SetRedirectionEntry(uint8_t pin, uint8_t vector, uint8_t dest_lapic_id, uint16_t flags, bool mask)
{
    uint32_t low_reg = 0x10 + (pin * 2);
    uint32_t high_reg = 0x11 + (pin * 2);

    uint32_t low_val = vector; // Delivery Mode = Fixed (000), Destination Mode = Physical (0)

    // Trigger Mode & Polarity 설정 (가독성 상수를 이용하여 개선)
    if ((flags & ISO_FLAGS_POLARITY_MASK) == ISO_FLAGS_POLARITY_ACTIVE_LOW)
    {
        low_val |= (1 << 13); // Active Low
    }
    if ((flags & ISO_FLAGS_TRIGGER_MASK) == ISO_FLAGS_TRIGGER_LEVEL)
    {
        low_val |= (1 << 15); // Level Triggered
    }

    if (mask)
    {
        low_val |= (1 << 16); // Masked
    }

    uint32_t high_val = (uint32_t)dest_lapic_id << 24;

    IOAPIC_Write(low_reg, low_val);
    IOAPIC_Write(high_reg, high_val);
}

void IOAPIC_Init(void)
{
    const ACPIInfo *acpi = ACPI_GetInfo();
    if (!acpi)
    {
        kPrintf("IOAPIC: Failed to get ACPI Info. Cannot configure IO APIC.\n");
        return;
    }

    kPrintf("IOAPIC: Configuring I/O APIC Redirection Table ( Symmmetric I/O Mode Activated )...\n");

    /* 
     * 기본적으로 ISA IRQ 0~15를 I/O APIC 핀 0~15에 1:1로 매핑.
     * 이때 기본 속성은 Edge Triggered, Active High, Masked 상태로 둡니다.
     * 단, 필요에 따라 마스크를 해제합니다.
     */
    for (uint8_t irq = 0; irq < 16; irq++)
    {
        // Vector = 0x20 (32) + irq
        // 기본값: Edge Triggered (0), Active High (0)
        IOAPIC_SetRedirectionEntry(irq, 0x20 + irq, 0, 0, true);
    }

    /* 
     * ACPI MADT의 Interrupt Source Override(ISO) 데이터 적용.
     * 버스 소스는 ISA(0)를 의미하며, 특정 IRQ가 다른 GSI 핀으로 리라우팅되거나 극성/트리거가 달라진 것을 세팅합니다.
     */
    for (int i = 0; i < acpi->iso_count; i++)
    {
        MADTIntSrcOverride iso = acpi->isos[i];
        if (iso.bus_source == 0) // ISA bus
        {
            // Vector = 0x20 + 원래 IRQ 번호 (기존 IDT 핸들러 구조 유지)
            // GSI가 곧 I/O APIC의 핀 번호
            kPrintf("IOAPIC: ISO Overriding ISA IRQ %u -> GSI %u, Flags=0x%04x\n", iso.irq_source, iso.gsi, iso.flags);
            
            // 타이머(IRQ 0)는 사용하지 않을 예정(Masked)이나 ISO 엔트리대로 위치만 변경해둡니다.
            // 키보드(IRQ 1)나 마우스(IRQ 12) 등은 적절한 ISO 핀에 덮어써지며, 
            // 나중에 명시적으로 unmask 처리를 수행합니다.
            bool mask_it = (iso.irq_source == 0) ? true : false;
            IOAPIC_SetRedirectionEntry(iso.gsi, 0x20 + iso.irq_source, 0, iso.flags, mask_it);
        }
    }

    /* 
     * 1:1 매핑되었거나 ISO로 재정의된 키보드(IRQ 1)와 마우스(IRQ 12)의 마스크를 확실하게 해제해 줍니다.
     * (만약 ISO에 걸려있지 않더라도 기본 1:1 핀 1번과 12번이 정상 작동하도록 보장)
     */
    // 키보드 IRQ 1 매핑 핀 찾기
    uint8_t kbd_pin = 1;
    uint16_t kbd_flags = 0;
    for (int i = 0; i < acpi->iso_count; i++)
    {
        if (acpi->isos[i].irq_source == 1 && acpi->isos[i].bus_source == 0)
        {
            kbd_pin = acpi->isos[i].gsi;
            kbd_flags = acpi->isos[i].flags;
            break;
        }
    }
    IOAPIC_SetRedirectionEntry(kbd_pin, 0x20 + 1, 0, kbd_flags, false); // Mask 해제 (Active)

    // 마우스 IRQ 12 매핑 핀 찾기
    uint8_t mouse_pin = 12;
    uint16_t mouse_flags = 0;
    for (int i = 0; i < acpi->iso_count; i++)
    {
        if (acpi->isos[i].irq_source == 12 && acpi->isos[i].bus_source == 0)
        {
            mouse_pin = acpi->isos[i].gsi;
            mouse_flags = acpi->isos[i].flags;
            break;
        }
    }
    IOAPIC_SetRedirectionEntry(mouse_pin, 0x20 + 12, 0, mouse_flags, false); // Mask 해제 (Active)

    kPrintf("IOAPIC: Keyboard (IRQ 1 -> GSI %u), Mouse (IRQ 12 -> GSI %u) Activated via I/O APIC.\n", kbd_pin, mouse_pin);
}

