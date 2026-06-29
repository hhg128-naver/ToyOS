#include "fixmap.h"
#include "vmm.h"
#include <stdio.h>

/*
 * Fixmap_Set: 지정 인덱스의 Fixmap 슬롯에 물리 주소를 매핑합니다.
 *
 * 내부적으로 VMM_MapPage를 호출하여 PML4→PDPT→PD→PT 4단계
 * 페이지 테이블을 자동 생성/갱신하고, invlpg로 TLB를 무효화합니다.
 */
void Fixmap_Set(enum fixed_addresses idx, uint64_t phys_addr, uint64_t flags)
{
    if (idx >= __end_of_fixed_addresses)
        return;

    void* vaddr = fix_to_virt(idx);

    /* 커널 마스터 PML4에 매핑 추가 */
    VMM_MapPage(vaddr, (void*)phys_addr, flags);

    /* 매핑 변경 후 해당 가상 주소의 TLB 엔트리 무효화 */
    InvalidatePage(vaddr);

    kPrintf("Fixmap: [%d] Virt %p -> Phys %p (flags=0x%lx)\n",
            idx, vaddr, (void*)phys_addr, flags);
}

/*
 * Fixmap_Clear: 지정 인덱스의 Fixmap 매핑을 해제합니다.
 *
 * TODO: VMM_UnmapPage 구현 후 PTE를 0으로 클리어하는 로직 추가.
 *       현재는 TLB 무효화만 수행합니다.
 */
void Fixmap_Clear(enum fixed_addresses idx)
{
    if (idx >= __end_of_fixed_addresses)
        return;

    void* vaddr = fix_to_virt(idx);
    InvalidatePage(vaddr);
}
