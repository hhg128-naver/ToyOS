#include "smp.h"
#include "apic.h"
#include "acpi.h"
#include "gdt.h"
#include "idt.h"
#include "pmm.h"
#include <stdio.h>
#include <string.h>

/* ap_trampoline_wrapper.asm에서 임베드된 트램펄린 바이너리 심볼 */
extern uint8_t ap_trampoline_bin_start[];
extern uint8_t ap_trampoline_bin_end[];

/* ===== 전역 변수 ===== */
CPUInfo      cpu_info[SMP_MAX_CPUS];
volatile int cpu_count_online = 0;

/* 전역 커널 스핀락 (스케줄러 등 공유 자원 보호용) */
spinlock_t g_kernel_lock = SPINLOCK_INIT;

/* ===== 스핀락 구현 ===== */

/*
 * spinlock_acquire: GCC/Clang의 __sync_lock_test_and_set 내장 함수를 사용한
 * 테스트-앤-셋 스핀락. 획득할 때까지 PAUSE 명령으로 busy-wait합니다.
 */
void spinlock_acquire(spinlock_t *lock)
{
    while (__sync_lock_test_and_set(lock, 1))
    {
        /* PAUSE: 하이퍼스레딩 파이프라인 효율을 개선하고 전력 소모를 줄임 */
        while (*lock)
        {
            __asm__ volatile("pause");
        }
    }
}

/*
 * spinlock_release: 메모리 배리어와 함께 락을 해제합니다.
 */
void spinlock_release(spinlock_t *lock)
{
    __sync_lock_release(lock);
}

/* ===== 내부 헬퍼 함수 ===== */

/*
 * smp_udelay: 포트 0x80 쓰기(약 1µs/회)를 이용한 마이크로초 단위 지연.
 * APIC IPI 프로토콜에서 INIT → SIPI 사이 10ms 대기에 사용됩니다.
 */
extern void outb(uint16_t port, uint8_t data);

static void smp_udelay(uint32_t us)
{
    for (uint32_t i = 0; i < us; i++)
    {
        outb(0x80, 0); /* POST 진단 포트 — 약 1µs 지연 */
    }
}

/* ===== AP 진입점 ===== */

/*
 * ap_entry: AP 트램펄린(ap_trampoline.asm)이 64비트 Long Mode 전환 완료 후
 *           jmp rax로 호출하는 C 진입점.
 *
 * 이 함수가 실행될 때의 상태:
 *   - CR3: BSP와 동일한 커널 PML4 (커널 주소 공간 공유)
 *   - RSP: SMP_Init()에서 PMM_AllocPage()로 할당한 AP 전용 커널 스택
 *   - CS/DS: 트램펄린 임시 GDT 기준 (0x08/0x10)
 *   - 인터럽트: CLI (비활성화 상태)
 *
 * 이 함수는 반환되지 않습니다.
 */
void ap_entry(void)
{
    /* 1. 이 AP의 Local APIC 활성화 (SVR, TPR 설정)
     *    MMIO 매핑은 BSP의 APIC_Init()에서 이미 완료됨 */
    APIC_Init_AP();

    /* 2. BSP의 GDT를 이 AP에 로드 (세그먼트 레지스터 갱신 포함) */
    InitGDT_AP();

    /* 3. BSP의 IDT를 이 AP에 로드 (예외 핸들링 활성화) */
    LoadIDT_AP();

    /* 4. FPU/SSE 활성화 (컴파일러가 printf 등에서 SSE 명령어를 사용하므로 필수) */
    uint64_t cr0 = ReadCR0();
    cr0 &= ~(1 << 2);  /* CR0.EM 해제 (x87 에뮬레이션 비활성화) */
    cr0 |= (1 << 1);   /* CR0.MP 설정 */
    WriteCR0(cr0);

    uint64_t cr4 = ReadCR4();
    cr4 |= (1 << 9);   /* CR4.OSFXSR (SSE 명령어 활성화) */
    cr4 |= (1 << 10);  /* CR4.OSXMMEXCPT (SSE 예외 처리 활성화) */
    WriteCR4(cr4);

    InitFPU();          /* fninit: FPU 상태 초기화 */

    uint32_t my_lapic_id = APIC_Read(APIC_ID_REG) >> 24;

    for (int i = 0; i < SMP_MAX_CPUS; i++)
    {
        if (cpu_info[i].lapic_id == (uint8_t)my_lapic_id && !cpu_info[i].online)
        {
            cpu_info[i].online = 1;
            break;
        }
    }

    /* 5. 온라인 CPU 카운트 원자적 증가 */
    __sync_fetch_and_add(&cpu_count_online, 1);

    /* 6. 이 AP의 APIC Timer 시작 (BSP의 캐리브레이션 결과 재사용) */
    APIC_Timer_Init(100); /* 100Hz, 벡터 48 */

    /* 7. 인터럽트 활성화 → APIC Timer가 타이머 인터럽트를 일으켜 Schedule()이 실행됨 */
    __asm__ volatile("sti");

    /* 8. idle 루프 (태스크가 없으면 hlt로 대기, 인터럽트로 깨어낙) */
    while (1)
    {
        __asm__ volatile("hlt");
    }
}

/* ===== SMP 초기화 ===== */

/*
 * SMP_Init: ACPI MADT에서 수집한 CPU 정보를 바탕으로
 *           BSP를 제외한 모든 AP를 순차적으로 깨웁니다.
 *
 * 프로토콜 (Intel MP Spec):
 *   1. AP 전용 커널 스택 할당
 *   2. 공유 부팅 데이터(APBootData) 작성
 *   3. 트램펄린 코드를 물리 주소 0x8000에 복사 (최초 1회)
 *   4. INIT IPI 전송 → 10ms 대기
 *   5. SIPI × 2 전송 → booted_flag 폴링
 *   6. 다음 AP로 반복
 */
void SMP_Init(void)
{
    const ACPIInfo *acpi = ACPI_GetInfo();

    /* BSP를 cpu_info[0]에 등록 — acpi 검사 전에 항상 수행 */
    uint32_t bsp_lapic_id = APIC_Read(APIC_ID_REG) >> 24;
    cpu_info[0].lapic_id         = (uint8_t)bsp_lapic_id;
    cpu_info[0].cpu_index        = 0;
    cpu_info[0].online           = 1;
    cpu_info[0].kernel_stack_top = 0;
    cpu_count_online             = 1;

    if (!acpi)
    {
        printf("SMP: ACPI info not found. Skipping AP boot.\n");
        return;
    }

    printf("\n=== SMP Initialization Start ===\n");
    printf("SMP: %d CPU(s) detected from ACPI MADT.\n", acpi->cpu_count);
    printf("SMP: BSP LAPIC ID = %u\n", bsp_lapic_id);

    if (acpi->cpu_count <= 1)
    {
        printf("SMP: Single-CPU system. No APs to start.\n");
        return;
    }

    /* ── 트램펄린 코드를 물리 주소 0x8000에 복사 ── */
    uint32_t trampoline_size = (uint32_t)(ap_trampoline_bin_end - ap_trampoline_bin_start);
    memcpy((void *)(uint64_t)AP_TRAMPOLINE_PHYS, ap_trampoline_bin_start, trampoline_size);
    printf("SMP: Trampoline copied to 0x%x (%u bytes).\n",
           AP_TRAMPOLINE_PHYS, trampoline_size);

    /* BSP의 PML4 물리 주소 (CR3) 읽기 */
    uint64_t pml4_phys;
    __asm__ volatile("mov %%cr3, %0" : "=r"(pml4_phys));


    /* ── 각 AP를 순차적으로 부팅 ── */
    int ap_num = 0;

    for (int i = 0; i < acpi->cpu_count; i++)
    {
        uint8_t lapic_id = acpi->cpu_apic_ids[i];

        /* BSP는 건너뜀 */
        if (lapic_id == (uint8_t)bsp_lapic_id)
            continue;

        ap_num++;
        int cpu_idx = ap_num; /* AP 인덱스 (1부터 시작) */

        if (cpu_idx >= SMP_MAX_CPUS)
        {
            printf("SMP: Max CPU count (%d) reached. Skipping remaining APs.\n", SMP_MAX_CPUS);
            break;
        }

        /* AP 전용 커널 스택 할당 (4KB = 1 페이지) */
        void *stack = PMM_AllocPage();
        if (!stack)
        {
            printf("SMP: Failed to allocate stack for AP #%d. Skipping.\n", cpu_idx);
            continue;
        }
        uint64_t stack_top = (uint64_t)stack + PAGE_SIZE;

        /* cpu_info 등록 */
        cpu_info[cpu_idx].lapic_id         = lapic_id;
        cpu_info[cpu_idx].cpu_index        = (uint8_t)cpu_idx;
        cpu_info[cpu_idx].online           = 0;
        cpu_info[cpu_idx].kernel_stack_top = stack_top;

        /* 공유 부팅 데이터 작성 */
        volatile APBootData *boot_data =
            (volatile APBootData *)(uint64_t)AP_BOOT_DATA_PHYS;

        boot_data->pml4_phys   = (uint32_t)pml4_phys;
        boot_data->stack_top   = stack_top;
        boot_data->entry_point = (uint64_t)ap_entry;
        boot_data->booted_flag = 0;

        printf("SMP: Starting AP #%d (LAPIC ID=%u)...\n", cpu_idx, lapic_id);

        /* ── INIT IPI 전송 ── */
        APIC_SendIPI(lapic_id, 0, APIC_IPI_INIT);
        smp_udelay(10000); /* 10ms 대기 */

        /* ── SIPI #1 전송 ── */
        APIC_SendIPI(lapic_id, AP_SIPI_VECTOR, APIC_IPI_STARTUP);
        smp_udelay(200);   /* 200µs 대기 */

        /* ── SIPI #2 전송 (첫 번째가 무시된 경우를 대비) ── */
        APIC_SendIPI(lapic_id, AP_SIPI_VECTOR, APIC_IPI_STARTUP);

        /* ── booted_flag 폴링 (최대 약 1초 대기) ── */
        int timeout = 1000;
        while (!boot_data->booted_flag && timeout > 0)
        {
            smp_udelay(1000); /* 1ms */
            timeout--;
        }

        if (boot_data->booted_flag)
        {
            printf("SMP: AP #%d (LAPIC ID=%u) trampoline done.\n", cpu_idx, lapic_id);
        }
        else
        {
            printf("SMP: AP #%d (LAPIC ID=%u) timed out — no response.\n", cpu_idx, lapic_id);
        }
    }

    /* 모든 AP가 ap_entry()를 완료할 시간을 잠시 줌 */
    smp_udelay(50000); /* 50ms */

    printf("\n=== SMP Initialization Complete ===\n");
    printf("SMP: Online CPUs: %d / %d\n", cpu_count_online, acpi->cpu_count);
    for (int i = 0; i < SMP_MAX_CPUS; i++)
    {
        if (cpu_info[i].lapic_id == 0 && i > 0)
            break;
        printf("  CPU #%d: LAPIC ID=%u, %s\n",
               i, cpu_info[i].lapic_id,
               cpu_info[i].online ? "Online" : "Offline");
    }
    printf("===================================\n\n");
}
