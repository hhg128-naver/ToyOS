OUTPUT_FORMAT(elf64-x86-64)
ENTRY(kmain)

SECTIONS
{
    /* 커널 로드 주소 (1MB) */
    . = 0x100000;

    .text ALIGN(0x1000) :
    {
        *(.text .text.*)       /* 모든 코드 섹션 포함 */
    }

    .rodata ALIGN(0x1000) :
    {
        *(.rodata .rodata.*)   /* 읽기 전용 데이터 포함 (printf 포맷 문자열 등) */
    }

    .init_array ALIGN(0x1000) :
    {
        PROVIDE_HIDDEN (__init_array_start = .);
        KEEP (*(SORT(.init_array.*)))
        KEEP (*(.init_array*))
        PROVIDE_HIDDEN (__init_array_end = .);
    }

    .data ALIGN(0x1000) :
    {
        *(.data .data.*)       /* 초기화된 데이터 포함 */
    }

    .bss ALIGN(0x1000) :
    {
        *(COMMON)
        *(.bss .bss.*)         /* 초기화되지 않은 데이터 포함 */
    }

    /* 기타 섹션들 (디버깅 정보 등 무시 가능하나 기본적으로 포함) */
    /DISCARD/ : {
        *(.comment)
        *(.eh_frame)
    }
}
