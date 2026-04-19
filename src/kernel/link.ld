OUTPUT_FORMAT(elf64-x86-64)
ENTRY(kmain)

SECTIONS
{
    /* 커널 로드 주소 (1MB) */
    . = 0x100000;

    .text :
    {
        *(.text .text.*)       /* 모든 코드 섹션 포함 */
    }

    .rodata :
    {
        *(.rodata .rodata.*)   /* 읽기 전용 데이터 포함 (printf 포맷 문자열 등) */
    }

    .data :
    {
        *(.data .data.*)       /* 초기화된 데이터 포함 */
    }

    .bss :
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
