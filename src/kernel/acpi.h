#ifndef ACPI_H
#define ACPI_H

#include "kernel.h"

/* ===== ACPI 테이블 시그니처 ===== */
#define ACPI_SIG_RSDP       "RSD PTR "  /* RSDP 시그니처 (8바이트) */
#define ACPI_SIG_MADT       0x43495041  /* "APIC" (리틀 엔디안) */

/* ===== MADT Entry 타입 ===== */
#define MADT_ENTRY_LOCAL_APIC           0   /* Processor Local APIC */
#define MADT_ENTRY_IO_APIC              1   /* I/O APIC */
#define MADT_ENTRY_INT_SRC_OVERRIDE     2   /* Interrupt Source Override */
#define MADT_ENTRY_NMI_SOURCE           3   /* NMI Source */
#define MADT_ENTRY_LOCAL_APIC_NMI       4   /* Local APIC NMI */
#define MADT_ENTRY_LOCAL_APIC_OVERRIDE  5   /* Local APIC Address Override */
#define MADT_ENTRY_IO_SAPIC             6   /* I/O SAPIC */
#define MADT_ENTRY_LOCAL_SAPIC          7   /* Local SAPIC */
#define MADT_ENTRY_PLATFORM_INT         8   /* Platform Interrupt Sources */
#define MADT_ENTRY_LOCAL_X2APIC         9   /* Processor Local x2APIC */
#define MADT_ENTRY_LOCAL_X2APIC_NMI     10  /* Local x2APIC NMI */

/* ===== Local APIC 플래그 ===== */
#define MADT_LAPIC_FLAG_ENABLED         (1 << 0)  /* 프로세서 활성화 */
#define MADT_LAPIC_FLAG_ONLINE_CAPABLE  (1 << 1)  /* 온라인 전환 가능 */

/*
 * RSDP (Root System Description Pointer) - ACPI 1.0
 * ACPI Specification, Section 5.2.5.1
 */
typedef struct __attribute__((packed))
{
    char     signature[8];      /* "RSD PTR " */
    uint8_t  checksum;          /* ACPI 1.0 부분의 체크섬 */
    char     oem_id[6];         /* OEM 식별 문자열 */
    uint8_t  revision;          /* ACPI 버전 (0=1.0, 2=2.0+) */
    uint32_t rsdt_address;      /* RSDT의 32비트 물리 주소 */
} RSDP;

/*
 * RSDP 확장 (ACPI 2.0+)
 * ACPI Specification, Section 5.2.5.3
 */
typedef struct __attribute__((packed))
{
    RSDP     rsdp10;            /* ACPI 1.0 호환 부분 */
    uint32_t length;            /* RSDP 전체 길이 (36바이트) */
    uint64_t xsdt_address;      /* XSDT의 64비트 물리 주소 */
    uint8_t  ext_checksum;      /* 전체 RSDP의 체크섬 */
    uint8_t  reserved[3];       /* 예약됨 */
} XSDP;

/*
 * ACPI SDT Header (System Description Table Header)
 * 모든 ACPI 테이블의 공통 헤더 (36바이트)
 */
typedef struct __attribute__((packed))
{
    uint32_t signature;         /* 테이블 시그니처 (예: "APIC", "FACP") */
    uint32_t length;            /* 테이블 전체 길이 (헤더 포함) */
    uint8_t  revision;          /* 테이블 리비전 */
    uint8_t  checksum;          /* 전체 테이블의 체크섬 (합 = 0) */
    char     oem_id[6];         /* OEM 식별 문자열 */
    char     oem_table_id[8];   /* OEM 테이블 ID */
    uint32_t oem_revision;      /* OEM 리비전 */
    uint32_t creator_id;        /* 생성 도구 벤더 ID */
    uint32_t creator_revision;  /* 생성 도구 리비전 */
} ACPISDTHeader;

/*
 * RSDT (Root System Description Table)
 * 32비트 포인터 배열을 포함하는 루트 테이블 (ACPI 1.0)
 */
typedef struct __attribute__((packed))
{
    ACPISDTHeader header;
    /* 이후에 uint32_t 배열이 동적으로 따라옴 */
} RSDT;

/*
 * XSDT (Extended System Description Table)
 * 64비트 포인터 배열을 포함하는 루트 테이블 (ACPI 2.0+)
 */
typedef struct __attribute__((packed))
{
    ACPISDTHeader header;
    /* 이후에 uint64_t 배열이 동적으로 따라옴 */
} XSDT;

/*
 * MADT (Multiple APIC Description Table)
 * 시그니처: "APIC"
 */
typedef struct __attribute__((packed))
{
    ACPISDTHeader header;
    uint32_t lapic_address;     /* Local APIC MMIO 물리 주소 */
    uint32_t flags;             /* 플래그 (bit 0: PCAT_COMPAT — 듀얼 8259 존재 여부) */
    /* 이후에 가변 길이 엔트리가 따라옴 */
} MADT;

/*
 * MADT Entry Header
 * 모든 MADT 엔트리의 공통 헤더 (2바이트)
 */
typedef struct __attribute__((packed))
{
    uint8_t type;               /* 엔트리 타입 */
    uint8_t length;             /* 엔트리 전체 길이 (헤더 포함) */
} MADTEntryHeader;

/*
 * Type 0: Processor Local APIC (8바이트)
 * 시스템의 각 논리 프로세서에 대한 정보
 */
typedef struct __attribute__((packed))
{
    MADTEntryHeader header;
    uint8_t  acpi_processor_id; /* ACPI 프로세서 ID */
    uint8_t  apic_id;           /* Local APIC ID */
    uint32_t flags;             /* 플래그 (bit 0: Enabled, bit 1: Online Capable) */
} MADTLocalAPIC;

/*
 * Type 1: I/O APIC (12바이트)
 * 시스템의 각 I/O APIC에 대한 정보
 */
typedef struct __attribute__((packed))
{
    MADTEntryHeader header;
    uint8_t  io_apic_id;        /* I/O APIC ID */
    uint8_t  reserved;          /* 예약됨 */
    uint32_t io_apic_address;   /* I/O APIC MMIO 물리 주소 */
    uint32_t gsi_base;          /* 이 I/O APIC이 처리하는 첫 GSI(Global System Interrupt) 번호 */
} MADTIOApic;

/*
 * Type 2: Interrupt Source Override (10바이트)
 * ISA IRQ와 GSI 매핑이 다른 경우의 재정의 정보
 */
typedef struct __attribute__((packed))
{
    MADTEntryHeader header;
    uint8_t  bus_source;        /* 항상 0 (ISA) */
    uint8_t  irq_source;       /* ISA IRQ 번호 */
    uint32_t gsi;               /* 실제 매핑되는 GSI 번호 */
    uint16_t flags;             /* 극성 및 트리거 모드 플래그 */
} MADTIntSrcOverride;

/*
 * Type 4: Local APIC NMI (6바이트)
 * NMI가 연결된 LAPIC LINT 핀 정보
 */
typedef struct __attribute__((packed))
{
    MADTEntryHeader header;
    uint8_t  acpi_processor_id; /* 대상 프로세서 (0xFF = 모든 프로세서) */
    uint16_t flags;             /* 극성 및 트리거 모드 플래그 */
    uint8_t  lint;              /* LINT 핀 번호 (0 또는 1) */
} MADTLocalAPICNMI;

/*
 * Type 5: Local APIC Address Override (12바이트)
 * 64비트 LAPIC 주소 재정의
 */
typedef struct __attribute__((packed))
{
    MADTEntryHeader header;
    uint16_t reserved;          /* 예약됨 */
    uint64_t lapic_address;     /* 64비트 Local APIC 물리 주소 */
} MADTLAPICAddrOverride;

/*
 * Type 9: Processor Local x2APIC (16바이트)
 * x2APIC 모드의 프로세서 정보
 */
typedef struct __attribute__((packed))
{
    MADTEntryHeader header;
    uint16_t reserved;          /* 예약됨 */
    uint32_t x2apic_id;         /* x2APIC ID */
    uint32_t flags;             /* 플래그 (bit 0: Enabled) */
    uint32_t acpi_processor_uid;/* ACPI 프로세서 UID */
} MADTLocalX2APIC;

/* ===== ACPI 정보 결과 구조체 ===== */
#define ACPI_MAX_CPUS           256
#define ACPI_MAX_IOAPICS        16
#define ACPI_MAX_ISO            32   /* Interrupt Source Override 최대 수 */

typedef struct
{
    /* CPU 정보 */
    int      cpu_count;                          /* 활성화된 CPU 수 */
    uint8_t  cpu_apic_ids[ACPI_MAX_CPUS];        /* 각 CPU의 Local APIC ID */

    /* I/O APIC 정보 */
    int      ioapic_count;                       /* I/O APIC 수 */
    MADTIOApic ioapics[ACPI_MAX_IOAPICS];        /* I/O APIC 엔트리 배열 */

    /* Interrupt Source Override 정보 */
    int      iso_count;                          /* ISO 수 */
    MADTIntSrcOverride isos[ACPI_MAX_ISO];       /* ISO 엔트리 배열 */

    /* Local APIC 정보 */
    uint64_t lapic_address;                      /* Local APIC 물리 주소 (Override 반영) */

    /* 시스템 플래그 */
    int      pcat_compat;                        /* 듀얼 8259 PIC 존재 여부 */

    /* ACPI 버전 */
    uint8_t  acpi_revision;                      /* RSDP 리비전 (0=1.0, 2=2.0+) */
} ACPIInfo;

/* ===== 함수 선언 ===== */

/**
 * ACPI_Init: RSDP로부터 RSDT/XSDT를 찾고 MADT를 파싱하여
 *            시스템의 APIC 토폴로지 정보를 수집합니다.
 *
 * @param boot_info: 부트로더가 전달한 BootInfo (RSDP 주소 포함)
 * @return 성공 시 0, 실패 시 -1
 */
int ACPI_Init(BootInfo *boot_info);

/**
 * ACPI_GetInfo: 파싱된 ACPI 정보를 반환합니다.
 *               ACPI_Init()이 성공적으로 호출된 후에만 유효합니다.
 *
 * @return ACPIInfo 구조체에 대한 포인터 (읽기 전용)
 */
const ACPIInfo* ACPI_GetInfo(void);

#endif
