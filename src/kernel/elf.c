#include "elf.h"
#include "vfs.h"
#include "vmm.h"
#include "pmm.h"
#include "kernel.h"
#include "heap.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* 이름으로 파일 노드를 찾는 헬퍼 함수 */
VFS_Node* VFS_FindFile(VFS_Node* parent, const char* name) {
    if (!parent) return NULL;
    int i = 0;
    while (1) {
        VFS_Node* node = VFS_ReadDir(parent, i++);
        if (node == NULL) break;
        if (strcmp(node->name, name) == 0) {
            return node;
        }
        free(node);
    }
    return NULL;
}

Task* LoadELFProcess(const char* filename, int arg) {
    if (vfs_root == NULL) {
        printf("ELF Loader: VFS Root is NULL\n");
        return NULL;
    }

    VFS_Node* target_node = VFS_FindFile(vfs_root, filename);
    if (!target_node) {
        printf("ELF Loader: File '%s' not found.\n", filename);
        return NULL;
    }

    if (target_node->flags & VFS_DIRECTORY) {
        printf("ELF Loader: '%s' is a directory.\n", filename);
        free(target_node);
        return NULL;
    }

    Elf64_Ehdr header;
    uint32_t bytes_read = VFS_Read(target_node, 0, sizeof(Elf64_Ehdr), (uint8_t*)&header);
    if (bytes_read != sizeof(Elf64_Ehdr)) {
        printf("ELF Loader: Failed to read ELF Header.\n");
        free(target_node);
        return NULL;
    }

    if (header.e_ident[0] != 0x7F || header.e_ident[1] != 'E' ||
        header.e_ident[2] != 'L'  || header.e_ident[3] != 'F') {
        printf("ELF Loader: Not a valid ELF file.\n");
        free(target_node);
        return NULL;
    }

    if (header.e_ident[4] != 2) {
        printf("ELF Loader: Not a 64-bit ELF.\n");
        free(target_node);
        return NULL;
    }

    if (header.e_machine != 62) {
        printf("ELF Loader: Not an x86_64 executable.\n");
        free(target_node);
        return NULL;
    }

    /* 프로그램 헤더 읽기 */
    uint64_t phdr_size = header.e_phnum * header.e_phentsize;
    Elf64_Phdr* phdrs = (Elf64_Phdr*)kmalloc(phdr_size);
    if (!phdrs) {
        printf("ELF Loader: Memory allocation failed.\n");
        free(target_node);
        return NULL;
    }
    VFS_Read(target_node, header.e_phoff, phdr_size, (uint8_t*)phdrs);

    /* 새 주소 공간 생성 */
    void* pml4 = VMM_CreateAddressSpace();
    if (!pml4) {
        printf("ELF Loader: Failed to create address space.\n");
        kfree(phdrs);
        free(target_node);
        return NULL;
    }

    /*
     * CR3 전환 없이 물리 주소에 직접 데이터를 복사하는 안전한 방식.
     * 커널의 0~1GB 영역은 Identity Mapped (가상 = 물리)이므로,
     * PMM_AllocPage()로 할당한 물리 페이지의 주소를 그대로 포인터로 사용합니다.
     * 이 방식은 CR3를 전환하지 않으므로 타이머 인터럽트에 의한
     * 컨텍스트 스위칭 간섭이 완전히 제거됩니다.
     */
    for (int i = 0; i < header.e_phnum; i++) {
        if (phdrs[i].p_type != 1) continue; // PT_LOAD만 처리

        uint64_t vaddr  = phdrs[i].p_vaddr;
        uint64_t memsz  = phdrs[i].p_memsz;
        uint64_t filesz = phdrs[i].p_filesz;
        uint64_t offset = phdrs[i].p_offset;

        uint64_t page_start = vaddr & ~0xFFFULL;
        uint64_t page_end   = (vaddr + memsz + 0xFFFULL) & ~0xFFFULL;

        for (uint64_t p = page_start; p < page_end; p += PAGE_SIZE) {
            void* phys = PMM_AllocPage();
            if (!phys) {
                printf("ELF Loader: Out of physical memory!\n");
                continue;
            }

            /* 물리 페이지를 0으로 초기화 (BSS 영역 대응) */
            memset(phys, 0, PAGE_SIZE);

            /* 이 페이지에 해당하는 파일 데이터가 있으면 물리 주소에 직접 복사 */
            if (filesz > 0) {
                uint64_t data_start  = vaddr;
                uint64_t data_end    = vaddr + filesz;
                uint64_t page_vstart = p;
                uint64_t page_vend   = p + PAGE_SIZE;

                uint64_t copy_start = (data_start > page_vstart) ? data_start : page_vstart;
                uint64_t copy_end   = (data_end < page_vend) ? data_end : page_vend;

                if (copy_start < copy_end) {
                    uint64_t file_off   = offset + (copy_start - vaddr);
                    uint64_t phys_off   = copy_start - p;
                    uint32_t copy_size  = (uint32_t)(copy_end - copy_start);
                    VFS_Read(target_node, file_off, copy_size, (uint8_t*)phys + phys_off);
                }
            }

            /* 유저 PML4에 가상→물리 매핑 등록 */
            VMM_MapPageEx(pml4, (void*)p, phys, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
        }
    }

    kfree(phdrs);
    free(target_node);

    /* 태스크 생성 및 큐에 등록 */
    Task* newTask = CreateELFTask(header.e_entry, arg, pml4);
    return newTask;
}
