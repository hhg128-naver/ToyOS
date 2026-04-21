#include "vmm.h"
#include "pmm.h"

static pt_entry* kernel_pml4;

void VMM_Init(BootInfo *binfo) {
    /* 새로운 PML4 생성 */
    kernel_pml4 = (pt_entry*)PMM_AllocPage();
    for (int i = 0; i < 512; i++) kernel_pml4[i] = 0;

    /* 
     * Identity Mapping 수행: 초기 커널과 주요 하드웨어 접근을 위해 
     * 물리 주소 0 ~ 1GB 영역을 가상 주소 0 ~ 1GB에 1:1로 매핑합니다.
     * 7단계 검증을 위해 이 영역에 PAGE_USER 권한을 부여합니다.
     */
    for (uint64_t addr = 0; addr < 0x40000000; addr += PAGE_SIZE) {
        VMM_MapPage((void*)addr, (void*)addr, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
    }

    /* 
     * 프레임버퍼 매핑: VMMSetup 이후에도 화면 출력이 가능하도록 
     * VRAM 영역을 반드시 페이지 테이블에 포함시켜야 합니다.
     * 유저 모드 시스템 콜(write)에서 Printf를 호출하므로 여기도 PAGE_USER를 줍니다.
     */
    uint64_t fb_base = (uint64_t)binfo->framebuffer;
    uint64_t fb_size = (uint64_t)binfo->horizontal_resolution * binfo->vertical_resolution * 4;
    for (uint64_t addr = fb_base; addr < fb_base + fb_size; addr += PAGE_SIZE) {
        VMM_MapPage((void*)addr, (void*)addr, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
    }

    /* CR3 레지스터에 새로운 페이지 테이블 로드 (asm_utils.asm에 정의됨) */
    LoadPageTable(kernel_pml4);
}

void VMM_MapPage(void* virtual_addr, void* physical_addr, uint64_t flags) {
    uint64_t v = (uint64_t)virtual_addr;
    uint64_t p = (uint64_t)physical_addr;

    /* 4단계 인덱스 계산 */
    uint64_t pml4_idx = (v >> 39) & 0x1FF;
    uint64_t pdpt_idx = (v >> 30) & 0x1FF;
    uint64_t pd_idx   = (v >> 21) & 0x1FF;
    uint64_t pt_idx   = (v >> 12) & 0x1FF;

    /* 
     * 중간 단계 테이블들은 하위 단계에서 요구하는 최대 권한을 허용해야 합니다.
     * 여기서는 flags에 포함된 PAGE_USER와 PAGE_WRITABLE을 상위 테이블 엔트리에도 누적 적용합니다.
     */
    uint64_t table_flags = PAGE_PRESENT | (flags & (PAGE_WRITABLE | PAGE_USER));

    /* 단계별 테이블 존재 확인 및 할당 */
    pt_entry* pml4 = kernel_pml4;
    if (!(pml4[pml4_idx] & PAGE_PRESENT)) {
        pml4[pml4_idx] = (uint64_t)PMM_AllocPage() | table_flags;
        pt_entry* new_table = (pt_entry*)(pml4[pml4_idx] & ~0xFFFULL);
        for(int i=0; i<512; i++) new_table[i] = 0;
    } else {
        pml4[pml4_idx] |= table_flags;
    }

    pt_entry* pdpt = (pt_entry*)(pml4[pml4_idx] & ~0xFFFULL);
    if (!(pdpt[pdpt_idx] & PAGE_PRESENT)) {
        pdpt[pdpt_idx] = (uint64_t)PMM_AllocPage() | table_flags;
        pt_entry* new_table = (pt_entry*)(pdpt[pdpt_idx] & ~0xFFFULL);
        for(int i=0; i<512; i++) new_table[i] = 0;
    } else {
        pdpt[pdpt_idx] |= table_flags;
    }

    pt_entry* pd = (pt_entry*)(pdpt[pdpt_idx] & ~0xFFFULL);
    if (!(pd[pd_idx] & PAGE_PRESENT)) {
        pd[pd_idx] = (uint64_t)PMM_AllocPage() | table_flags;
        pt_entry* new_table = (pt_entry*)(pd[pd_idx] & ~0xFFFULL);
        for(int i=0; i<512; i++) new_table[i] = 0;
    } else {
        pd[pd_idx] |= table_flags;
    }

    pt_entry* pt = (pt_entry*)(pd[pd_idx] & ~0xFFFULL);
    pt[pt_idx] = p | flags | PAGE_PRESENT;
}
