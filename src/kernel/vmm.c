#include "vmm.h"
#include "pmm.h"
#include <stdio.h>

static pt_entry* kernel_pml4;

void VMM_Init(BootInfo *binfo) {
    /* 새로운 PML4 생성 */
    kernel_pml4 = (pt_entry*)PMM_AllocPage();
    if (!kernel_pml4) return;
    for (int i = 0; i < 512; i++) kernel_pml4[i] = 0;

    /* 
     * Identity Mapping 수행: 초기 커널과 주요 하드웨어 접근을 위해 
     * 물리 주소 0 ~ 4GB 영역을 가상 주소 0 ~ 4GB에 1:1로 매핑합니다.
     * (기존 1GB에서 4GB로 확장하여 더 많은 물리 메모리에 직접 접근 가능하게 함)
     */
    for (uint64_t addr = 0; addr < 0x100000000ULL; addr += PAGE_SIZE) {
        VMM_MapPage((void*)addr, (void*)addr, PAGE_PRESENT | PAGE_WRITABLE);
    }

    /* 
     * 프레임버퍼 매핑: VMMSetup 이후에도 화면 출력이 가능하도록 
     * VRAM 영역을 반드시 페이지 테이블에 포함시켜야 합니다.
     */
    uint64_t fb_base = (uint64_t)binfo->framebuffer;
    uint64_t fb_size = (uint64_t)binfo->horizontal_resolution * binfo->vertical_resolution * 4;
    for (uint64_t addr = fb_base; addr < fb_base + fb_size; addr += PAGE_SIZE) {
        VMM_MapPage((void*)addr, (void*)addr, PAGE_PRESENT | PAGE_WRITABLE);
    }

    /* CR3 레지스터에 새로운 페이지 테이블 로드 (asm_utils.asm에 정의됨) */
    LoadPageTable(kernel_pml4);
}

void VMM_MapPage(void* virtual_addr, void* physical_addr, uint64_t flags) {
    VMM_MapPageEx(kernel_pml4, virtual_addr, physical_addr, flags);
}

void VMM_MapPageEx(pt_entry* pml4, void* virtual_addr, void* physical_addr, uint64_t flags) {
    uint64_t v = (uint64_t)virtual_addr;
    uint64_t p = (uint64_t)physical_addr;

    /* 4단계 인덱스 계산 */
    uint64_t pml4_idx = (v >> 39) & 0x1FF;
    uint64_t pdpt_idx = (v >> 30) & 0x1FF;
    uint64_t pd_idx   = (v >> 21) & 0x1FF;
    uint64_t pt_idx   = (v >> 12) & 0x1FF;

    uint64_t table_flags = PAGE_PRESENT | (flags & (PAGE_WRITABLE | PAGE_USER));

    /* 1. PML4 -> PDPT */
    if (!(pml4[pml4_idx] & PAGE_PRESENT)) {
        void* page = PMM_AllocPage();
        if (!page) return;
        pml4[pml4_idx] = (uint64_t)page | table_flags;
        pt_entry* new_table = (pt_entry*)page;
        for(int i=0; i<512; i++) new_table[i] = 0;
    } else {
        /* 유저 매핑을 추가하려는데 기존 테이블이 커널 전용(shared)이면 복제 수행 (CoW) */
        if ((flags & PAGE_USER) && !(pml4[pml4_idx] & PAGE_USER)) {
            void* old_table = (void*)(pml4[pml4_idx] & 0x000FFFFFFFFFF000ULL);
            void* new_table = PMM_AllocPage();
            if (new_table) {
                for(int i=0; i<512; i++) ((pt_entry*)new_table)[i] = ((pt_entry*)old_table)[i];
                pml4[pml4_idx] = (uint64_t)new_table | table_flags;
            }
        } else {
            pml4[pml4_idx] |= table_flags;
        }
    }

    /* 2. PDPT -> PD */
    pt_entry* pdpt = (pt_entry*)(pml4[pml4_idx] & 0x000FFFFFFFFFF000ULL);
    if (!(pdpt[pdpt_idx] & PAGE_PRESENT)) {
        void* page = PMM_AllocPage();
        if (!page) return;
        pdpt[pdpt_idx] = (uint64_t)page | table_flags;
        pt_entry* new_table = (pt_entry*)page;
        for(int i=0; i<512; i++) new_table[i] = 0;
    } else {
        if ((flags & PAGE_USER) && !(pdpt[pdpt_idx] & PAGE_USER)) {
            void* old_table = (void*)(pdpt[pdpt_idx] & 0x000FFFFFFFFFF000ULL);
            void* new_table = PMM_AllocPage();
            if (new_table) {
                for(int i=0; i<512; i++) ((pt_entry*)new_table)[i] = ((pt_entry*)old_table)[i];
                pdpt[pdpt_idx] = (uint64_t)new_table | table_flags;
            }
        } else {
            pdpt[pdpt_idx] |= table_flags;
        }
    }

    /* 3. PD -> PT */
    pt_entry* pd = (pt_entry*)(pdpt[pdpt_idx] & 0x000FFFFFFFFFF000ULL);
    if (!(pd[pd_idx] & PAGE_PRESENT)) {
        void* page = PMM_AllocPage();
        if (!page) return;
        pd[pd_idx] = (uint64_t)page | table_flags;
        pt_entry* new_table = (pt_entry*)page;
        for(int i=0; i<512; i++) new_table[i] = 0;
    } else {
        if ((flags & PAGE_USER) && !(pd[pd_idx] & PAGE_USER)) {
            void* old_table = (void*)(pd[pd_idx] & 0x000FFFFFFFFFF000ULL);
            void* new_table = PMM_AllocPage();
            if (new_table) {
                for(int i=0; i<512; i++) ((pt_entry*)new_table)[i] = ((pt_entry*)old_table)[i];
                pd[pd_idx] = (uint64_t)new_table | table_flags;
            }
        } else {
            pd[pd_idx] |= table_flags;
        }
    }

    /* 4. PT -> Data Page */
    pt_entry* pt = (pt_entry*)(pd[pd_idx] & 0x000FFFFFFFFFF000ULL);
    pt[pt_idx] = p | flags | PAGE_PRESENT;
}

void* VMM_CreateAddressSpace() {
    /* 새로운 PML4 할당 */
    pt_entry* pml4 = (pt_entry*)PMM_AllocPage();
    if (!pml4) return NULL;
    for (int i = 0; i < 512; i++) pml4[i] = 0;

    /* PML4의 0번 엔트리(0~512GB)를 위한 독립된 PDPT 생성 */
    pt_entry* new_pdpt = (pt_entry*)PMM_AllocPage();
    if (!new_pdpt) {
        PMM_FreePage(pml4);
        return NULL;
    }
    for (int i = 0; i < 512; i++) new_pdpt[i] = 0;

    /* 마스터 커널 PDPT의 엔트리를 복사하되, PD 레벨까지 딥 카피 */
    pt_entry* master_pdpt = (pt_entry*)(kernel_pml4[0] & 0x000FFFFFFFFFF000ULL);
    for (int i = 0; i < 512; i++) {
        if (master_pdpt[i] & PAGE_PRESENT) {
            pt_entry* master_pd = (pt_entry*)(master_pdpt[i] & 0x000FFFFFFFFFF000ULL);
            pt_entry* new_pd = (pt_entry*)PMM_AllocPage();
            if (!new_pd) continue; 
            
            for (int j = 0; j < 512; j++) {
                new_pd[j] = master_pd[j];
            }
            /* semi-shared entries에도 PAGE_USER를 줘서 VMM_FreeAddressSpace에서 추적 가능하게 함 */
            new_pdpt[i] = (uint64_t)new_pd | (master_pdpt[i] & 0xFFFULL) | PAGE_USER;
        }
    }

    /* 새 PML4의 0번 엔트리가 새 PDPT를 가리키도록 설정 */
    pml4[0] = (uint64_t)new_pdpt | PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;

    /* 나머지 PML4 엔트리들도 마스터에서 복사 (보통은 0 이외엔 비어있음) */
    for (int i = 1; i < 512; i++) {
        pml4[i] = kernel_pml4[i];
    }

    return pml4;
}

static void VMM_FreeRecursive(pt_entry* table, int level) {
    if (!table) return;

    for (int i = 0; i < 512; i++) {
        if (table[i] & PAGE_PRESENT) {
            /* PAGE_USER 비트가 있는 경우에만 유저가 소유한 페이지로 간주하고 해제 */
            if (table[i] & PAGE_USER) {
                void* child = (void*)(table[i] & 0x000FFFFFFFFFF000ULL);
                if (!child) continue;

                /* 
                 * 레벨 정의: 
                 * level 0: PML4 (child: PDPT)
                 * level 1: PDPT (child: PD)
                 * level 2: PD   (child: PT)
                 * level 3: PT   (child: Data Page)
                 * level < 3일 때만 재귀 호출하여 PT 하위의 데이터 페이지까지 도달합니다.
                 */
                if (level < 3) {
                    VMM_FreeRecursive((pt_entry*)child, level + 1);
                }
                
                /* 자식 페이지(PD, PT, 또는 실제 데이터 페이지) 해제 */
                PMM_FreePage(child);
                table[i] = 0; // 중복 해제 방지
            }
        }
    }
}

void VMM_FreeAddressSpace(void* pml4_phys) {
    if (!pml4_phys || pml4_phys == kernel_pml4) return;

    pt_entry* pml4 = (pt_entry*)pml4_phys;
    
    /* PML4의 모든 엔트리를 순회하며 PAGE_USER가 설정된 것들을 재귀적으로 해제 */
    VMM_FreeRecursive(pml4, 0);

    /* PML4 자체 해제 */
    PMM_FreePage(pml4_phys);
}
