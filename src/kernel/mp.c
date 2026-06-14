#include "mp.h"
#include <stdio.h>
#include <string.h>

/* 파싱 결과를 저장하는 전역 변수 */
static MPInfo mp_info;
static int    mp_initialized = 0;

/*
 * MP_Checksum: 지정된 메모리 영역의 바이트 합을 계산합니다.
 * MP 구조체의 유효성을 검증하기 위해 사용됩니다.
 * 유효한 구조체는 모든 바이트의 합이 0이어야 합니다.
 *
 * @param addr: 검사할 메모리 시작 주소
 * @param len:  검사할 바이트 수
 * @return 바이트 합 (0이면 유효)
 */
static uint8_t MP_Checksum(void *addr, int len)
{
    uint8_t sum = 0;
    uint8_t *p = (uint8_t *)addr;
    for (int i = 0; i < len; i++)
    {
        sum += p[i];
    }
    return sum;
}

/*
 * MP_SearchRange: 지정된 메모리 범위에서 MP Floating Pointer Structure를 탐색합니다.
 *
 * "_MP_" 시그니처(4바이트)를 16바이트 경계에서 검색합니다.
 * MP Floating Pointer Structure는 항상 16바이트 경계에 정렬되어 있습니다.
 *
 * @param start: 탐색 시작 물리 주소
 * @param len:   탐색 범위 (바이트)
 * @return 발견된 MPFloatingPointer 포인터, 못 찾으면 NULL
 */
static MPFloatingPointer* MP_SearchRange(uint64_t start, uint32_t len)
{
    uint8_t *addr = (uint8_t *)start;
    uint8_t *end  = addr + len;

    /* 16바이트 경계로 정렬 */
    for (; addr < end; addr += 16)
    {
        /* 시그니처 비교: "_MP_" */
        if (*(uint32_t *)addr == MP_FPS_SIGNATURE)
        {
            MPFloatingPointer *fps = (MPFloatingPointer *)addr;

            /* 구조체 길이 검증 (length 필드 * 16 = 실제 크기, 보통 16바이트) */
            if (fps->length != 1)
                continue;

            /* 체크섬 검증 */
            if (MP_Checksum(fps, 16) != 0)
                continue;

            kPrintf("MP: Found MP Floating Pointer at %p (Rev 1.%d)\n",
                   (void *)addr, fps->spec_rev);
            return fps;
        }
    }

    return NULL;
}

/*
 * MP_FindFloatingPointer: 시스템 메모리에서 MP Floating Pointer Structure를 찾습니다.
 *
 * Intel MP Specification 1.4에 따라 다음 세 영역을 순서대로 탐색합니다:
 * 1. EBDA (Extended BIOS Data Area)의 첫 1KB
 * 2. 시스템 기본 메모리 마지막 1KB (0x9FC00 ~ 0x9FFFF)
 * 3. BIOS ROM 영역 (0xF0000 ~ 0xFFFFF)
 *
 * @return 발견된 MPFloatingPointer 포인터, 못 찾으면 NULL
 */
static MPFloatingPointer* MP_FindFloatingPointer(void)
{
    MPFloatingPointer *fps = NULL;

    /*
     * 1. EBDA (Extended BIOS Data Area) 탐색
     *    BDA(BIOS Data Area)의 0x040E 위치에 EBDA 세그먼트 주소가 저장되어 있습니다.
     *    세그먼트 주소에 16을 곱하면 실제 물리 주소가 됩니다.
     */
    uint16_t ebda_seg = *(uint16_t *)0x040E;
    if (ebda_seg != 0)
    {
        uint64_t ebda_addr = (uint64_t)ebda_seg << 4;
        kPrintf("MP: Searching EBDA at %p...\n", (void *)ebda_addr);
        fps = MP_SearchRange(ebda_addr, 1024);
        if (fps)
            return fps;
    }

    /*
     * 2. 시스템 기본 메모리 마지막 1KB (0x9FC00 ~ 0x9FFFF)
     *    일부 시스템에서는 EBDA 포인터가 없거나 잘못된 경우를 대비합니다.
     */
    kPrintf("MP: Searching last 1KB of base memory (0x9FC00)...\n");
    fps = MP_SearchRange(0x9FC00, 1024);
    if (fps)
        return fps;

    /*
     * 3. BIOS ROM 영역 (0xF0000 ~ 0xFFFFF, 64KB)
     *    BIOS 코드/데이터 영역에서 탐색합니다.
     */
    kPrintf("MP: Searching BIOS ROM area (0xF0000)...\n");
    fps = MP_SearchRange(0xF0000, 0x10000);
    if (fps)
        return fps;

    return NULL;
}

/*
 * MP_ParseConfigTable: MP Configuration Table을 파싱하여 시스템 정보를 수집합니다.
 *
 * @param table: MP Configuration Table 헤더의 포인터
 * @return 성공 시 0, 실패 시 -1
 */
static int MP_ParseConfigTable(MPConfigTable *table)
{
    /* 시그니처 검증: "PCMP" */
    if (table->signature != MP_CFG_SIGNATURE)
    {
        kPrintf("MP: Invalid Config Table signature: 0x%x\n", table->signature);
        return -1;
    }

    /* 체크섬 검증 */
    if (MP_Checksum(table, table->base_table_length) != 0)
    {
        kPrintf("MP: Config Table checksum failed.\n");
        return -1;
    }

    /* 테이블 헤더 정보 출력 */
    char oem_id[9] = {0};
    char product_id[13] = {0};
    memcpy(oem_id, table->oem_id, 8);
    memcpy(product_id, table->product_id, 12);

    kPrintf("MP Config Table:\n");
    kPrintf("  OEM: '%s', Product: '%s'\n", oem_id, product_id);
    kPrintf("  Spec Rev: 1.%d, Entry Count: %u\n", table->spec_rev, table->entry_count);
    kPrintf("  Local APIC Addr: 0x%08x\n", table->lapic_addr);

    /* Local APIC 주소 저장 */
    mp_info.lapic_addr = table->lapic_addr;

    /* 엔트리 파싱: 헤더 바로 뒤부터 시작 */
    uint8_t *entry_ptr = (uint8_t *)table + sizeof(MPConfigTable);
    uint16_t count = table->entry_count;

    for (uint16_t i = 0; i < count; i++)
    {
        uint8_t entry_type = *entry_ptr;

        switch (entry_type)
        {
        case MP_ENTRY_PROCESSOR:
        {
            MPProcessorEntry *proc = (MPProcessorEntry *)entry_ptr;
            if (mp_info.cpu_count < MP_MAX_CPUS)
            {
                mp_info.cpus[mp_info.cpu_count] = *proc;
                mp_info.cpu_count++;
            }

            const char *bsp_str = (proc->cpu_flags & MP_CPU_FLAG_BSP) ? "BSP" : "AP";
            const char *en_str  = (proc->cpu_flags & MP_CPU_FLAG_ENABLED) ? "Enabled" : "Disabled";

            kPrintf("  CPU #%d: LAPIC ID=%u, %s, %s\n",
                   mp_info.cpu_count - 1, proc->lapic_id, bsp_str, en_str);

            /* BSP의 LAPIC ID 기록 */
            if (proc->cpu_flags & MP_CPU_FLAG_BSP)
            {
                mp_info.bsp_lapic_id = proc->lapic_id;
            }

            entry_ptr += 20; /* Processor Entry는 20바이트 */
            break;
        }

        case MP_ENTRY_BUS:
        {
            MPBusEntry *bus = (MPBusEntry *)entry_ptr;
            char bus_type[7] = {0};
            memcpy(bus_type, bus->bus_type, 6);
            kPrintf("  Bus #%d: Type='%s'\n", bus->bus_id, bus_type);
            entry_ptr += 8; /* Bus Entry는 8바이트 */
            break;
        }

        case MP_ENTRY_IOAPIC:
        {
            MPIOAPICEntry *ioapic = (MPIOAPICEntry *)entry_ptr;
            if (mp_info.ioapic_count < MP_MAX_IOAPICS)
            {
                mp_info.ioapics[mp_info.ioapic_count] = *ioapic;
                mp_info.ioapic_count++;
            }

            const char *en_str = (ioapic->ioapic_flags & 0x01) ? "Enabled" : "Disabled";
            kPrintf("  I/O APIC #%d: ID=%u, Ver=%u, Addr=0x%08x, %s\n",
                   mp_info.ioapic_count - 1, ioapic->ioapic_id,
                   ioapic->ioapic_version, ioapic->ioapic_addr, en_str);

            entry_ptr += 8; /* I/O APIC Entry는 8바이트 */
            break;
        }

        case MP_ENTRY_IO_INT:
        {
            /* I/O Interrupt Assignment — 정보 로그만 출력 */
            MPIOIntEntry *io_int = (MPIOIntEntry *)entry_ptr;
            (void)io_int; /* 미사용 경고 방지 */
            entry_ptr += 8;
            break;
        }

        case MP_ENTRY_LOCAL_INT:
        {
            /* Local Interrupt Assignment — 정보 로그만 출력 */
            MPLocalIntEntry *local_int = (MPLocalIntEntry *)entry_ptr;
            (void)local_int; /* 미사용 경고 방지 */
            entry_ptr += 8;
            break;
        }

        default:
            kPrintf("MP: Unknown entry type %d at offset 0x%x\n",
                   entry_type, (uint32_t)(entry_ptr - (uint8_t *)table));
            return -1;
        }
    }

    return 0;
}

/*
 * MP_Init: MP Floating Pointer Structure를 탐색하고
 *          MP Configuration Table을 파싱합니다.
 *
 * @return 성공 시 0, 실패 시 -1
 */
int MP_Init(void)
{
    /* 결과 구조체 초기화 */
    memset(&mp_info, 0, sizeof(MPInfo));

    kPrintf("=== MP Configuration Table Detection ===\n");

    /* 1. MP Floating Pointer Structure 찾기 */
    MPFloatingPointer *fps = MP_FindFloatingPointer();
    if (!fps)
    {
        kPrintf("MP: MP Floating Pointer Structure not found.\n");
        kPrintf("MP: This system may not support the MP Specification.\n");
        kPrintf("MP: Consider using ACPI MADT instead.\n");
        return -1;
    }

    kPrintf("MP: Spec Revision: 1.%d\n", fps->spec_rev);
    kPrintf("MP: Feature Byte 1: 0x%02x\n", fps->mp_feature1);
    kPrintf("MP: Feature Byte 2: 0x%02x\n", fps->mp_feature2);

    /* IMCR 존재 여부 확인 (Feature Byte 2의 bit 7) */
    mp_info.imcr_present = (fps->mp_feature2 & 0x80) ? 1 : 0;
    if (mp_info.imcr_present)
    {
        kPrintf("MP: IMCR (Interrupt Mode Configuration Register) present.\n");
    }

    /* 2. MP Configuration Table 파싱 */
    if (fps->mp_feature1 != 0)
    {
        /*
         * mp_feature1 != 0이면 Default Configuration을 사용합니다.
         * 이 경우 MP Config Table이 없으며 미리 정의된 구성을 사용합니다.
         * (현재는 간단히 듀얼 프로세서로 가정)
         */
        kPrintf("MP: Using Default Configuration Type %d\n", fps->mp_feature1);
        kPrintf("MP: Default Configuration parsing not fully implemented.\n");

        /* 최소한 BSP 하나는 등록 */
        mp_info.cpu_count = 1;
        mp_info.cpus[0].entry_type = MP_ENTRY_PROCESSOR;
        mp_info.cpus[0].lapic_id = 0;
        mp_info.cpus[0].cpu_flags = MP_CPU_FLAG_ENABLED | MP_CPU_FLAG_BSP;
        mp_info.bsp_lapic_id = 0;
        mp_info.lapic_addr = 0xFEE00000;
    }
    else
    {
        /* mp_feature1 == 0이면 mp_config_ptr이 유효한 Config Table을 가리킵니다 */
        if (fps->mp_config_ptr == 0)
        {
            kPrintf("MP: Config Table pointer is NULL.\n");
            return -1;
        }

        MPConfigTable *table = (MPConfigTable *)(uint64_t)fps->mp_config_ptr;
        kPrintf("MP: Config Table at %p\n", (void *)table);

        if (MP_ParseConfigTable(table) != 0)
        {
            kPrintf("MP: Failed to parse Config Table.\n");
            return -1;
        }
    }

    /* 결과 요약 출력 */
    kPrintf("\n=== MP Detection Summary ===\n");
    kPrintf("  Total CPUs:     %d\n", mp_info.cpu_count);
    kPrintf("  Total I/O APICs:%d\n", mp_info.ioapic_count);
    kPrintf("  BSP LAPIC ID:   %u\n", mp_info.bsp_lapic_id);
    kPrintf("  LAPIC Address:  0x%08x\n", mp_info.lapic_addr);
    kPrintf("  IMCR Present:   %s\n", mp_info.imcr_present ? "Yes" : "No");
    kPrintf("============================\n");

    mp_initialized = 1;
    return 0;
}

/*
 * MP_GetInfo: 파싱된 MP 정보를 반환합니다.
 *
 * @return MPInfo 구조체에 대한 포인터, 초기화 전이면 NULL
 */
const MPInfo* MP_GetInfo(void)
{
    if (!mp_initialized)
        return NULL;
    return &mp_info;
}
