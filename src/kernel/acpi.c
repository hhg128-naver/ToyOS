#include "acpi.h"
#include <stdio.h>
#include <string.h>

/* 파싱 결과를 저장하는 전역 변수 */
static ACPIInfo acpi_info;
static int      acpi_initialized = 0;

/*
 * ACPI_Checksum: 지정된 메모리 영역의 바이트 합을 계산합니다.
 * ACPI 테이블의 유효성 검증에 사용됩니다.
 * 유효한 테이블은 모든 바이트의 합이 0이어야 합니다.
 *
 * @param addr: 검사할 메모리 시작 주소
 * @param len:  검사할 바이트 수
 * @return 바이트 합 (0이면 유효)
 */
static uint8_t ACPI_Checksum(void *addr, uint32_t len)
{
    uint8_t sum = 0;
    uint8_t *p = (uint8_t *)addr;
    for (uint32_t i = 0; i < len; i++)
    {
        sum += p[i];
    }
    return sum;
}

/*
 * ACPI_ValidateRSDP: RSDP 구조체의 유효성을 검증합니다.
 *
 * 1. 시그니처 "RSD PTR " 확인 (8바이트)
 * 2. ACPI 1.0 부분(20바이트)의 체크섬 검증
 * 3. ACPI 2.0+인 경우 확장 부분(36바이트)의 체크섬도 검증
 *
 * @param rsdp: RSDP 포인터
 * @return 유효하면 1, 아니면 0
 */
static int ACPI_ValidateRSDP(RSDP *rsdp)
{
    /* 시그니처 검증: "RSD PTR " */
    if (memcmp(rsdp->signature, ACPI_SIG_RSDP, 8) != 0)
    {
        kPrintf("ACPI: Invalid RSDP signature.\n");
        return 0;
    }

    /* ACPI 1.0 부분 체크섬 (처음 20바이트) */
    if (ACPI_Checksum(rsdp, 20) != 0)
    {
        kPrintf("ACPI: RSDP checksum (v1.0) failed.\n");
        return 0;
    }

    /* ACPI 2.0+ 확장 체크섬 (전체 36바이트) */
    if (rsdp->revision >= 2)
    {
        XSDP *xsdp = (XSDP *)rsdp;
        if (ACPI_Checksum(xsdp, xsdp->length) != 0)
        {
            kPrintf("ACPI: XSDP extended checksum failed.\n");
            return 0;
        }
    }

    return 1;
}

/*
 * ACPI_FindTable: RSDT 또는 XSDT에서 지정된 시그니처의 ACPI 테이블을 찾습니다.
 *
 * @param rsdp: 유효한 RSDP 포인터
 * @param signature: 찾고자 하는 4바이트 시그니처 (uint32_t)
 * @return 발견된 테이블의 SDT 헤더 포인터, 못 찾으면 NULL
 */
static ACPISDTHeader* ACPI_FindTable(RSDP *rsdp, uint32_t signature)
{
    if (rsdp->revision >= 2)
    {
        /*
         * ACPI 2.0+: XSDT 사용 (64비트 포인터)
         */
        XSDP *xsdp = (XSDP *)rsdp;
        XSDT *xsdt = (XSDT *)(uintptr_t)xsdp->xsdt_address;

        if (!xsdt)
        {
            kPrintf("ACPI: XSDT address is NULL, falling back to RSDT.\n");
            goto use_rsdt;
        }

        /* XSDT 체크섬 검증 */
        if (ACPI_Checksum(xsdt, xsdt->header.length) != 0)
        {
            kPrintf("ACPI: XSDT checksum failed, falling back to RSDT.\n");
            goto use_rsdt;
        }

        /* XSDT 엔트리 수 계산 (각 엔트리는 64비트 포인터) */
        int entry_count = (xsdt->header.length - sizeof(ACPISDTHeader)) / sizeof(uint64_t);
        uint64_t *entries = (uint64_t *)((uint8_t *)xsdt + sizeof(ACPISDTHeader));

        kPrintf("ACPI: XSDT at %p, %d entries\n", (void *)xsdt, entry_count);

        for (int i = 0; i < entry_count; i++)
        {
            ACPISDTHeader *header = (ACPISDTHeader *)(uintptr_t)entries[i];
            if (header && header->signature == signature)
            {
                return header;
            }
        }

        kPrintf("ACPI: Table 0x%08x not found in XSDT.\n", signature);
        return NULL;
    }

use_rsdt:
    /*
     * ACPI 1.0 또는 XSDT 폴백: RSDT 사용 (32비트 포인터)
     */
    {
        RSDT *rsdt = (RSDT *)(uintptr_t)rsdp->rsdt_address;

        if (!rsdt)
        {
            kPrintf("ACPI: RSDT address is NULL.\n");
            return NULL;
        }

        /* RSDT 체크섬 검증 */
        if (ACPI_Checksum(rsdt, rsdt->header.length) != 0)
        {
            kPrintf("ACPI: RSDT checksum failed.\n");
            return NULL;
        }

        /* RSDT 엔트리 수 계산 (각 엔트리는 32비트 포인터) */
        int entry_count = (rsdt->header.length - sizeof(ACPISDTHeader)) / sizeof(uint32_t);
        uint32_t *entries = (uint32_t *)((uint8_t *)rsdt + sizeof(ACPISDTHeader));

        kPrintf("ACPI: RSDT at %p, %d entries\n", (void *)rsdt, entry_count);

        for (int i = 0; i < entry_count; i++)
        {
            ACPISDTHeader *header = (ACPISDTHeader *)(uintptr_t)entries[i];
            if (header && header->signature == signature)
            {
                return header;
            }
        }

        kPrintf("ACPI: Table 0x%08x not found in RSDT.\n", signature);
        return NULL;
    }
}

/*
 * ACPI_ParseMADT: MADT(Multiple APIC Description Table)를 파싱합니다.
 *
 * MADT에는 시스템의 인터럽트 컨트롤러 토폴로지가 기술되어 있습니다:
 * - Local APIC (각 프로세서에 대한 정보)
 * - I/O APIC (인터럽트 라우팅 컨트롤러)
 * - Interrupt Source Override (ISA→GSI 매핑 변경)
 * - Local APIC NMI (NMI 연결 정보)
 *
 * @param madt: MADT 테이블 포인터
 * @return 성공 시 0, 실패 시 -1
 */
static int ACPI_ParseMADT(MADT *madt)
{
    /* 체크섬 검증 */
    if (ACPI_Checksum(madt, madt->header.length) != 0)
    {
        kPrintf("ACPI: MADT checksum failed.\n");
        return -1;
    }

    /* MADT 헤더 정보 출력 */
    kPrintf("ACPI MADT (Multiple APIC Description Table):\n");
    kPrintf("  Local APIC Address: 0x%08x\n", madt->lapic_address);
    kPrintf("  Flags: 0x%08x", madt->flags);
    if (madt->flags & 0x01)
    {
        kPrintf(" (Dual 8259 PICs present)");
        acpi_info.pcat_compat = 1;
    }
    kPrintf("\n");

    /* Local APIC 기본 주소 저장 */
    acpi_info.lapic_address = (uint64_t)madt->lapic_address;

    /* 엔트리 파싱: MADT 헤더 뒤부터 테이블 끝까지 */
    uint8_t* madt_entry_ptr = (uint8_t *)madt + sizeof(MADT);
    uint8_t* madt_table_end = (uint8_t *)madt + madt->header.length;

    while (madt_entry_ptr < madt_table_end)
    {
        MADTEntryHeader* madt_entry = (MADTEntryHeader*)madt_entry_ptr;

        /* 안전 검사: 엔트리 길이가 0이면 무한 루프 방지 */
        if (madt_entry->length < 2)
        {
            kPrintf("ACPI: Invalid MADT entry length (%d) at offset 0x%x\n", madt_entry->length, (uint32_t)(madt_entry_ptr - (uint8_t *)madt));
            break;
        }

        switch (madt_entry->type)
        {
        case MADT_ENTRY_LOCAL_APIC:
        {
            MADTLocalAPIC* local_apic = (MADTLocalAPIC*)madt_entry_ptr;

            /*
             * Enabled 플래그 또는 Online Capable 플래그가 설정된 CPU만 등록합니다.
             * Online Capable은 현재는 비활성이지만 OS가 온라인으로 전환할 수 있는 CPU입니다.
             */
            int usable = (local_apic->flags & MADT_LAPIC_FLAG_ENABLED) ||
                         (local_apic->flags & MADT_LAPIC_FLAG_ONLINE_CAPABLE);

            const char *status = (local_apic->flags & MADT_LAPIC_FLAG_ENABLED) ? "Enabled" :
                                 (local_apic->flags & MADT_LAPIC_FLAG_ONLINE_CAPABLE) ? "Online Capable" :
                                 "Disabled";

            kPrintf("  [Local APIC] Processor ID=%u, APIC ID=%u, %s\n", local_apic->acpi_processor_id, local_apic->apic_id, status);

            if (usable && acpi_info.cpu_count < ACPI_MAX_CPUS)
            {
                acpi_info.cpu_apic_ids[acpi_info.cpu_count] = local_apic->apic_id;
                acpi_info.cpu_count++;
            }
            break;
        }

        case MADT_ENTRY_IO_APIC:
        {
            MADTIOApic* io_apic = (MADTIOApic*)madt_entry_ptr;

            kPrintf("  [I/O APIC] ID=%u, Address=0x%08x, GSI Base=%u\n", io_apic->io_apic_id, io_apic->io_apic_address, io_apic->gsi_base);

            if (acpi_info.ioapic_count < ACPI_MAX_IOAPICS)
            {
                acpi_info.ioapics[acpi_info.ioapic_count] = *io_apic;
                acpi_info.ioapic_count++;
            }
            break;
        }

        case MADT_ENTRY_INT_SRC_OVERRIDE:
        {
            MADTIntSrcOverride* iso = (MADTIntSrcOverride*)madt_entry_ptr;

            kPrintf("  [Int Source Override] Bus=%u, IRQ=%u -> GSI=%u, Flags=0x%04x\n", iso->bus_source, iso->irq_source, iso->gsi, iso->flags);

            if (acpi_info.iso_count < ACPI_MAX_ISO)
            {
                acpi_info.isos[acpi_info.iso_count] = *iso;
                acpi_info.iso_count++;
            }
            break;
        }

        case MADT_ENTRY_NMI_SOURCE:
        {
            kPrintf("  [NMI Source] (length=%u)\n", madt_entry->length);
            break;
        }

        case MADT_ENTRY_LOCAL_APIC_NMI:
        {
            MADTLocalAPICNMI* nmi = (MADTLocalAPICNMI*)madt_entry_ptr;

            const char *target = (nmi->acpi_processor_id == 0xFF) ? "All CPUs" : "Specific";
            kPrintf("  [Local APIC NMI] Processor=%s (ID=%u), LINT#%u, Flags=0x%04x\n", target, nmi->acpi_processor_id, nmi->lint, nmi->flags);
            break;
        }

        case MADT_ENTRY_LOCAL_APIC_OVERRIDE:
        {
            MADTLAPICAddrOverride* local_apic_override = (MADTLAPICAddrOverride*)madt_entry_ptr;

            kPrintf("  [LAPIC Address Override] 0x%08x%08x\n",
                   (uint32_t)(local_apic_override->lapic_address >> 32),
                   (uint32_t)(local_apic_override->lapic_address & 0xFFFFFFFF));

            /* 64비트 주소로 Local APIC 주소 갱신 */
            acpi_info.lapic_address = local_apic_override->lapic_address;
            break;
        }

        case MADT_ENTRY_LOCAL_X2APIC:
        {
            MADTLocalX2APIC *x2apic = (MADTLocalX2APIC *)madt_entry_ptr;

            const char *status = (x2apic->flags & MADT_LAPIC_FLAG_ENABLED) ? "Enabled" : "Disabled";
            kPrintf("  [Local x2APIC] UID=%u, x2APIC ID=%u, %s\n", x2apic->acpi_processor_uid, x2apic->x2apic_id, status);

            /* x2APIC ID는 8비트를 초과할 수 있으므로 별도 처리가 필요할 수 있음 */
            if ((x2apic->flags & MADT_LAPIC_FLAG_ENABLED) && acpi_info.cpu_count < ACPI_MAX_CPUS && x2apic->x2apic_id <= 0xFF)
            {
                acpi_info.cpu_apic_ids[acpi_info.cpu_count] = (uint8_t)x2apic->x2apic_id;
                acpi_info.cpu_count++;
            }
            break;
        }

        default:
            kPrintf("  [Unknown MADT Entry] Type=%u, Length=%u\n", madt_entry->type, madt_entry->length);
            break;
        }

        madt_entry_ptr += madt_entry->length;
    }

    return 0;
}

/*
 * ACPI_Init: RSDP → RSDT/XSDT → MADT 경로로 ACPI 테이블을 파싱합니다.
 *
 * @param boot_info: 부트로더가 전달한 BootInfo (RSDP 주소 포함)
 * @return 성공 시 0, 실패 시 -1
 */
int ACPI_Init(BootInfo *boot_info)
{
    /* 결과 구조체 초기화 */
    memset(&acpi_info, 0, sizeof(ACPIInfo));

    kPrintf("\n=== ACPI Table Detection ===\n");

    /* 1. RSDP 확인 */
    if (!boot_info->rsdp)
    {
        kPrintf("ACPI: RSDP not provided by bootloader.\n");
        return -1;
    }

    RSDP *rsdp = (RSDP *)boot_info->rsdp;
    kPrintf("ACPI: RSDP at %p\n", (void *)rsdp);

    /* RSDP 유효성 검증 */
    if (!ACPI_ValidateRSDP(rsdp))
    {
        return -1;
    }

    /* OEM ID 출력 */
    char oem_id[7] = {0};
    memcpy(oem_id, rsdp->oem_id, 6);
    kPrintf("ACPI: OEM='%s', Revision=%u (ACPI %s)\n",
           oem_id, rsdp->revision,
           rsdp->revision >= 2 ? "2.0+" : "1.0");

    acpi_info.acpi_revision = rsdp->revision;

    /* 2. MADT 찾기 */
    ACPISDTHeader *madt_header = ACPI_FindTable(rsdp, ACPI_SIG_MADT);
    if (!madt_header)
    {
        kPrintf("ACPI: MADT (APIC table) not found.\n");
        return -1;
    }

    kPrintf("ACPI: MADT found at %p (Length: %u bytes)\n",
           (void *)madt_header, madt_header->length);

    /* 3. MADT 파싱 */
    MADT *madt = (MADT *)madt_header;
    if (ACPI_ParseMADT(madt) != 0)
    {
        kPrintf("ACPI: Failed to parse MADT.\n");
        return -1;
    }

    /* 결과 요약 출력 */
    kPrintf("\n=== ACPI MADT Summary ===\n");
    kPrintf("  ACPI Revision:     %s\n", acpi_info.acpi_revision >= 2 ? "2.0+" : "1.0");
    kPrintf("  Total CPUs:        %d\n", acpi_info.cpu_count);
    kPrintf("  Total I/O APICs:   %d\n", acpi_info.ioapic_count);
    kPrintf("  IRQ Overrides:     %d\n", acpi_info.iso_count);
    kPrintf("  LAPIC Address:     %p\n", (void *)acpi_info.lapic_address);
    kPrintf("  Dual 8259 PICs:    %s\n", acpi_info.pcat_compat ? "Yes" : "No");
    kPrintf("=========================\n\n");

    acpi_initialized = 1;
    return 0;
}

/*
 * ACPI_GetInfo: 파싱된 ACPI 정보를 반환합니다.
 *
 * @return ACPIInfo 구조체에 대한 포인터, 초기화 전이면 NULL
 */
const ACPIInfo* ACPI_GetInfo(void)
{
    if (!acpi_initialized)
        return NULL;
    return &acpi_info;
}
