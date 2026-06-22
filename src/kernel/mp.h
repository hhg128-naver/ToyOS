#ifndef MP_H
#define MP_H

#include "kernel.h"

/* ===== MP Floating Pointer Structure 시그니처 ===== */
#define MP_FPS_SIGNATURE        0x5F504D5F  /* "_MP_" (리틀 엔디안) */

/* ===== MP Configuration Table 시그니처 ===== */
#define MP_CFG_SIGNATURE        0x504D4350  /* "PCMP" (리틀 엔디안) */

/* ===== MP Configuration Table Entry 타입 ===== */
#define MP_ENTRY_PROCESSOR      0   /* Processor entry */
#define MP_ENTRY_BUS            1   /* Bus entry */
#define MP_ENTRY_IOAPIC         2   /* I/O APIC entry */
#define MP_ENTRY_IO_INT         3   /* I/O Interrupt Assignment */
#define MP_ENTRY_LOCAL_INT      4   /* Local Interrupt Assignment */

/* ===== Processor Entry CPU 플래그 ===== */
#define MP_CPU_FLAG_ENABLED     (1 << 0)  /* 이 프로세서가 활성화 상태 */
#define MP_CPU_FLAG_BSP         (1 << 1)  /* Bootstrap Processor */

/*
 * MP Floating Pointer Structure (16바이트)
 * Intel MP Specification 1.4, Section 4.1
 *
 * 메모리의 특정 영역을 스캔하여 "_MP_" 시그니처로 이 구조체를 찾은 뒤,
 * mp_config_ptr 필드가 가리키는 MP Configuration Table을 파싱합니다.
 */
typedef struct __attribute__((packed))
{
    uint32_t signature;          /* "_MP_" (0x5F504D5F) */
    uint32_t mp_config_ptr;      /* MP Configuration Table의 물리 주소 */
    uint8_t  length;             /* 이 구조체의 길이 (16바이트 단위, 항상 1) */
    uint8_t  spec_rev;           /* MP Spec 버전 (0x01=1.1, 0x04=1.4) */
    uint8_t  checksum;           /* 전체 구조체의 체크섬 (합 = 0) */
    uint8_t  mp_feature1;        /* Feature Byte 1 (Default Configuration 타입) */
    uint8_t  mp_feature2;        /* Feature Byte 2 (IMCR 존재 여부 등) */
    uint8_t  mp_feature3;        /* 예약됨 */
    uint8_t  mp_feature4;        /* 예약됨 */
    uint8_t  mp_feature5;        /* 예약됨 */
} MPFloatingPointer;

/*
 * MP Configuration Table Header (44바이트)
 * Intel MP Specification 1.4, Section 4.2
 */
typedef struct __attribute__((packed))
{
    uint32_t signature;          /* "PCMP" (0x504D4350) */
    uint16_t base_table_length;  /* 베이스 테이블 전체 길이 (헤더 포함) */
    uint8_t  spec_rev;           /* MP Spec 버전 */
    uint8_t  checksum;           /* 베이스 테이블 체크섬 (합 = 0) */
    char     oem_id[8];          /* OEM 식별 문자열 */
    char     product_id[12];     /* 제품 식별 문자열 */
    uint32_t oem_table_ptr;      /* OEM 테이블 포인터 (없으면 0) */
    uint16_t oem_table_size;     /* OEM 테이블 크기 */
    uint16_t entry_count;        /* 베이스 테이블 엔트리 수 */
    uint32_t lapic_addr;         /* Local APIC MMIO 베이스 주소 */
    uint16_t ext_table_length;   /* 확장 테이블 길이 */
    uint8_t  ext_table_checksum; /* 확장 테이블 체크섬 */
    uint8_t  reserved;           /* 예약됨 */
} MPConfigTable;

/*
 * Processor Entry (20바이트)
 * MP Configuration Table의 Entry Type 0
 */
typedef struct __attribute__((packed))
{
    uint8_t  entry_type;         /* 항상 0 (Processor) */
    uint8_t  lapic_id;           /* Local APIC ID */
    uint8_t  lapic_version;      /* Local APIC 버전 */
    uint8_t  cpu_flags;          /* CPU 플래그 (EN, BSP) */
    uint32_t cpu_signature;      /* CPU 시그니처 (Stepping, Model, Family) */
    uint32_t feature_flags;      /* CPUID Feature Flags */
    uint64_t reserved;           /* 예약됨 */
} MPProcessorEntry;

/*
 * Bus Entry (8바이트)
 * MP Configuration Table의 Entry Type 1
 */
typedef struct __attribute__((packed))
{
    uint8_t  entry_type;         /* 항상 1 (Bus) */
    uint8_t  bus_id;             /* 버스 ID */
    char     bus_type[6];        /* 버스 타입 문자열 (예: "ISA   ", "PCI   ") */
} MPBusEntry;

/*
 * I/O APIC Entry (8바이트)
 * MP Configuration Table의 Entry Type 2
 */
typedef struct __attribute__((packed))
{
    uint8_t  entry_type;         /* 항상 2 (I/O APIC) */
    uint8_t  ioapic_id;          /* I/O APIC ID */
    uint8_t  ioapic_version;     /* I/O APIC 버전 */
    uint8_t  ioapic_flags;       /* 플래그 (bit 0: EN = 활성화) */
    uint32_t ioapic_addr;        /* I/O APIC MMIO 베이스 주소 */
} MPIOAPICEntry;

/*
 * I/O Interrupt Assignment Entry (8바이트)
 * MP Configuration Table의 Entry Type 3
 */
typedef struct __attribute__((packed))
{
    uint8_t  entry_type;         /* 항상 3 (I/O Interrupt) */
    uint8_t  interrupt_type;     /* 인터럽트 타입 (0=INT, 1=NMI, 2=SMI, 3=ExtINT) */
    uint16_t io_int_flags;       /* 인터럽트 플래그 (극성, 트리거 모드) */
    uint8_t  src_bus_id;         /* 소스 버스 ID */
    uint8_t  src_bus_irq;        /* 소스 버스 IRQ */
    uint8_t  dest_ioapic_id;     /* 대상 I/O APIC ID */
    uint8_t  dest_ioapic_intin;  /* 대상 I/O APIC INTIN# 핀 */
} MPIOIntEntry;

/*
 * Local Interrupt Assignment Entry (8바이트)
 * MP Configuration Table의 Entry Type 4
 */
typedef struct __attribute__((packed))
{
    uint8_t  entry_type;         /* 항상 4 (Local Interrupt) */
    uint8_t  interrupt_type;     /* 인터럽트 타입 (0=INT, 1=NMI, 2=SMI, 3=ExtINT) */
    uint16_t local_int_flags;    /* 인터럽트 플래그 */
    uint8_t  src_bus_id;         /* 소스 버스 ID */
    uint8_t  src_bus_irq;        /* 소스 버스 IRQ */
    uint8_t  dest_lapic_id;      /* 대상 Local APIC ID (0xFF = 모든 LAPIC) */
    uint8_t  dest_lapic_lintin;  /* 대상 LAPIC LINT# 핀 (0 또는 1) */
} MPLocalIntEntry;

/* ===== MP 정보 결과 구조체 ===== */
#define MP_MAX_CPUS     16
#define MP_MAX_IOAPICS  8

typedef struct
{
    int              cpu_count;                      /* 발견된 CPU 수 */
    MPProcessorEntry cpus[MP_MAX_CPUS];              /* CPU 엔트리 배열 */
    int              ioapic_count;                   /* 발견된 I/O APIC 수 */
    MPIOAPICEntry    ioapics[MP_MAX_IOAPICS];        /* I/O APIC 엔트리 배열 */
    uint32_t         lapic_addr;                     /* Local APIC 베이스 주소 */
    uint8_t          bsp_lapic_id;                   /* BSP의 Local APIC ID */
    int              imcr_present;                   /* IMCR 존재 여부 (Feature Byte 2, bit 7) */
} MPInfo;

/* ===== 함수 선언 ===== */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * MP_Init: MP Floating Pointer Structure를 탐색하고
 *          MP Configuration Table을 파싱하여 시스템의 멀티프로세서 정보를 수집합니다.
 *
 * @return 성공 시 0, 실패 시 -1
 */
int MP_Init(void);

/**
 * MP_GetInfo: 파싱된 MP 정보를 반환합니다.
 *             MP_Init()이 성공적으로 호출된 후에만 유효한 결과를 반환합니다.
 *
 * @return MPInfo 구조체에 대한 포인터 (읽기 전용)
 */
const MPInfo* MP_GetInfo(void);

#ifdef __cplusplus
}
#endif

#endif
