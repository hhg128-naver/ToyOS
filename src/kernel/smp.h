#ifndef SMP_H
#define SMP_H

#include "kernel.h"

/* ===== 상수 정의 ===== */

#define CONFIG_SMP              1       /* 1: 멀티코어(SMP) 활성화, 0: 싱글코어 전용 */
#define SMP_MAX_CPUS            16      /* 지원 최대 CPU 수 */
#define AP_BOOT_DATA_PHYS       0x7F00  /* 공유 부팅 데이터 물리 주소 */
#define AP_TRAMPOLINE_PHYS      0x8000  /* AP 트램펄린 물리 주소 (SIPI 벡터 << 12) */
#define AP_SIPI_VECTOR          0x08    /* SIPI 벡터: 0x08 << 12 = 0x8000 */

/*
 * APBootData: BSP가 AP를 깨우기 전에 물리 주소 AP_BOOT_DATA_PHYS에 기록하는
 * 공유 데이터 구조체. 레이아웃은 ap_trampoline.asm의 AP_BOOT_DATA 오프셋과
 * 정확히 일치해야 합니다.
 *
 *  오프셋  크기  설명
 *  +0      4B    pml4_phys    - CR3에 로드할 PML4 물리 주소
 *  +4      4B    (패딩)
 *  +8      8B    stack_top    - AP의 초기 커널 스택 최상단
 *  +16     8B    entry_point  - ap_entry() 함수 64비트 주소
 *  +24     4B    booted_flag  - AP가 부팅 데이터 소비 후 1로 설정
 */
typedef struct __attribute__((packed))
{
    uint32_t          pml4_phys;        /* +0:  CR3 값 (PML4 물리 주소) */
    uint32_t          _pad;             /* +4:  패딩 */
    uint64_t          stack_top;        /* +8:  AP 커널 스택 최상단 */
    uint64_t          entry_point;      /* +16: ap_entry() 주소 */
    volatile uint32_t booted_flag;      /* +24: 트램펄린 완료 플래그 */
} APBootData;

/*
 * CPUInfo: CPU 코어별 상태 정보.
 * cpu_info[0] = BSP, cpu_info[1..n] = APs
 */
typedef struct
{
    uint8_t       lapic_id;         /* Local APIC ID */
    uint8_t       cpu_index;        /* 커널 내부 CPU 인덱스 */
    volatile int  online;           /* 온라인 여부 (AP가 설정) */
    uint64_t      kernel_stack_top; /* AP 커널 스택 최상단 */
} CPUInfo;

#include "spinlock.h"

/* ===== 전역 변수 ===== */

extern CPUInfo      cpu_info[SMP_MAX_CPUS];
extern volatile int cpu_count_online;   /* 현재 온라인 CPU 수 (BSP=1 포함) */
extern spinlock_t   g_kernel_lock;      /* 전역 커널 스핀락 */

/* ===== 함수 선언 ===== */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * SMP_Init: ACPI 정보를 기반으로 모든 AP(Application Processor)를
 *           순차적으로 깨웁니다 (INIT-SIPI-SIPI 프로토콜).
 *           AP는 깨어난 후 ap_entry()를 실행하고 idle 루프에 진입합니다.
 *
 * 전제 조건: ACPI_Init(), APIC_Init() 이후에 호출해야 합니다.
 */
void SMP_Init(void);

/**
 * ap_entry: AP가 64비트 Long Mode로 진입한 직후 호출되는 C 진입점.
 *           ap_trampoline.asm에서 jmp rax로 직접 호출됩니다.
 *           반환되지 않습니다.
 */
void ap_entry(void);

#ifdef __cplusplus
}
#endif

#endif /* SMP_H */
