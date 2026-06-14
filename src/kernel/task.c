#include "task.h"
#include "kernel.h"
#include "heap.h"
#include "gdt.h"
#include "vmm.h"
#include "pmm.h"
#include "smp.h"
#include "spinlock.h"
#include "apic.h"
#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>

extern BootInfo *boot_info_global;

static Task* tasks[MAX_TASKS];
static int task_count = 0;

/*
 * per_cpu_task_idx: LAPIC ID를 인덱스로 사용하는 per-CPU 태스크 인덱스 배열.
 * 값 -1은 해당 CPU가 아직 태스크를 실행하지 않는 상태를 의미합니다.
 */
static int per_cpu_task_idx[SMP_MAX_CPUS];

uint64_t current_kernel_stack_top_array[SMP_MAX_CPUS] = {0};

/* 현재 CPU의 LAPIC ID 반환 (클램핑 포함) */
static inline uint32_t get_cpu_id(void)
{
    uint32_t id = APIC_Read(APIC_ID_REG) >> 24;
    return (id < SMP_MAX_CPUS) ? id : 0;
}

void InitializeTaskSystem() {
    /* per-CPU 인덱스 배열 초기화 (모두 -1: 태스크 없음) */
    for (int i = 0; i < SMP_MAX_CPUS; i++)
        per_cpu_task_idx[i] = -1;

    // 메인 커널 흐름을 0번 태스크로 등록
    Task* mainTask = (Task*)kmalloc(sizeof(Task));
    if (!mainTask) return;
    
    mainTask->id = task_count++;
    mainTask->state = TASK_RUNNING;
    mainTask->stack_base = NULL;
    mainTask->kernel_stack_top = 0;
    mainTask->rsp = 0;
    mainTask->pml4 = GetCR3();
    
    for (int i = 0; i < MAX_TASKS; i++)
        tasks[i] = NULL;
    tasks[0] = mainTask;

    /* BSP의 LAPIC ID 를 확인하여 BSP의 태스크 인덱스를 0번으로 설정 */
    uint32_t bsp_lapic_id = get_cpu_id();
    per_cpu_task_idx[bsp_lapic_id] = 0;
    
    uint64_t dummy_rsp;
    __asm__ volatile("mov %%rsp, %0" : "=r"(dummy_rsp));
    current_kernel_stack_top_array[bsp_lapic_id] = dummy_rsp;
    
    kPrintf("Task System Initialized.\n");
}

Task* CreateTask(void (*entryPoint)()) {
    uint64_t flags = spinlock_lock_irqsave(&g_kernel_lock);

    if (task_count >= MAX_TASKS) {
        kPrintf("Error: Max tasks reached.\n");
        spinlock_unlock_irqrestore(&g_kernel_lock, flags);
        return NULL;
    }

    Task* newTask = (Task*)kmalloc(sizeof(Task));
    if (!newTask) {
        spinlock_unlock_irqrestore(&g_kernel_lock, flags);
        return NULL;
    }
    
    void* stack = kmalloc(TASK_STACK_SIZE);
    if (!stack) {
        kfree(newTask);
        spinlock_unlock_irqrestore(&g_kernel_lock, flags);
        return NULL;
    }

    newTask->id = task_count;
    newTask->state = TASK_READY;
    newTask->pml4 = tasks[0]->pml4;
    newTask->stack_base = stack;
    
    uint64_t stack_top = (uint64_t)stack + TASK_STACK_SIZE;
    newTask->kernel_stack_top = stack_top;
    
    Context* ctx = (Context*)(stack_top - sizeof(Context));
    ctx->rip = (uint64_t)entryPoint;
    ctx->cs = 0x08;
    ctx->rflags = 0x202;
    ctx->rsp = stack_top;
    ctx->ss = 0x10;
    ctx->rax = ctx->rbx = ctx->rcx = ctx->rdx = 0;
    ctx->rsi = ctx->rdi = ctx->rbp = 0;
    ctx->r8 = ctx->r9 = ctx->r10 = ctx->r11 = 0;
    ctx->r12 = ctx->r13 = ctx->r14 = ctx->r15 = 0;
    ctx->interrupt_number = 0;
    ctx->error_code = 0;

    newTask->rsp = (uint64_t)ctx;
    newTask->heap_start = 0x60000000;
    newTask->heap_end = 0x60000000;
    tasks[task_count++] = newTask;
    
    spinlock_unlock_irqrestore(&g_kernel_lock, flags);
    kPrintf("Created New Task.\n");
    return newTask;
}

Task* CreateUserTask(void (*entryPoint)(), int arg) {
    uint64_t flags = spinlock_lock_irqsave(&g_kernel_lock);

    if (task_count >= MAX_TASKS) {
        kPrintf("Error: Max tasks reached.\n");
        spinlock_unlock_irqrestore(&g_kernel_lock, flags);
        return NULL;
    }

    Task* newTask = (Task*)kmalloc(sizeof(Task));
    if (!newTask) {
        spinlock_unlock_irqrestore(&g_kernel_lock, flags);
        return NULL;
    }
    
    void* pml4 = VMM_CreateAddressSpace();
    if (!pml4) {
        kfree(newTask);
        spinlock_unlock_irqrestore(&g_kernel_lock, flags);
        return NULL;
    }

    void* kstack = kmalloc(TASK_STACK_SIZE);
    if (!kstack) {
        VMM_FreeAddressSpace(pml4);
        kfree(newTask);
        spinlock_unlock_irqrestore(&g_kernel_lock, flags);
        return NULL;
    }

    void* ustack_phys = PMM_AllocPage();
    if (!ustack_phys) {
        kfree(kstack);
        VMM_FreeAddressSpace(pml4);
        kfree(newTask);
        spinlock_unlock_irqrestore(&g_kernel_lock, flags);
        return NULL;
    }
    void* ustack_virt = (void*)0x00007FFFFFFF0000;
    VMM_MapPageEx(pml4, ustack_virt, ustack_phys, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
    uint64_t ustack_top = (uint64_t)ustack_virt + PAGE_SIZE;

    uint64_t code_page = (uint64_t)entryPoint & ~0xFFFULL;
    VMM_MapPageEx(pml4, (void*)code_page, (void*)code_page, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
    VMM_MapPageEx(pml4, (void*)(code_page + PAGE_SIZE), (void*)(code_page + PAGE_SIZE), PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);

    uint64_t kstack_top = (uint64_t)kstack + TASK_STACK_SIZE;
    newTask->id = task_count;
    newTask->state = TASK_READY;
    newTask->pml4 = pml4;
    newTask->stack_base = kstack;
    newTask->kernel_stack_top = kstack_top;
    
    Context* ctx = (Context*)(kstack_top - sizeof(Context));
    ctx->rip = (uint64_t)entryPoint;
    ctx->cs = 0x23;
    ctx->rflags = 0x202;
    ctx->rsp = ustack_top;
    ctx->ss = 0x1B;
    ctx->rax = ctx->rbx = ctx->rcx = ctx->rdx = 0;
    ctx->rsi = 0;
    ctx->rdi = (uint64_t)arg;
    ctx->rbp = 0;
    ctx->r8 = ctx->r9 = ctx->r10 = ctx->r11 = 0;
    ctx->r12 = ctx->r13 = ctx->r14 = ctx->r15 = 0;
    ctx->interrupt_number = 0;
    ctx->error_code = 0;

    newTask->rsp = (uint64_t)ctx;
    newTask->heap_start = 0x60000000;
    newTask->heap_end = 0x60000000;
    tasks[task_count++] = newTask;
    
    spinlock_unlock_irqrestore(&g_kernel_lock, flags);
    kPrintf("Created Isolated User Task with arg: %d, PML4: %p\n", arg, newTask->pml4);
    return newTask;
}

Task* CreateELFTask(uint64_t entryPoint, int arg, void* pml4) {
    uint64_t flags = spinlock_lock_irqsave(&g_kernel_lock);

    int slot = -1;
    for (int i = 0; i < MAX_TASKS; i++)
    {
        if (tasks[i] == NULL)
        {
            slot = i;
            break;
        }
    }

    if (slot == -1) {
        kPrintf("Error: Max tasks reached.\n");
        spinlock_unlock_irqrestore(&g_kernel_lock, flags);
        return NULL;
    }

    void* kstack = kmalloc(TASK_STACK_SIZE);
    if (!kstack) {
        spinlock_unlock_irqrestore(&g_kernel_lock, flags);
        return NULL;
    }
    
    void* ustack_phys = PMM_AllocPage();
    if (!ustack_phys) {
        kfree(kstack);
        spinlock_unlock_irqrestore(&g_kernel_lock, flags);
        return NULL;
    }
    void* ustack_virt = (void*)0x00007FFFFFFF0000;
    VMM_MapPageEx(pml4, ustack_virt, ustack_phys, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
    uint64_t ustack_top = (uint64_t)ustack_virt + PAGE_SIZE;

    Task* newTask = tasks[slot];
    if (newTask == NULL) {
        newTask = (Task*)kmalloc(sizeof(Task));
        if (!newTask) {
            PMM_FreePage(ustack_phys);
            kfree(kstack);
            spinlock_unlock_irqrestore(&g_kernel_lock, flags);
            return NULL;
        }
        tasks[slot] = newTask;
        if (slot >= task_count) task_count = slot + 1;
    }
    
    newTask->id = slot;
    newTask->pml4 = pml4;
    newTask->stack_base = kstack;
    uint64_t kstack_top = (uint64_t)kstack + TASK_STACK_SIZE;
    newTask->kernel_stack_top = kstack_top;
    
    Context* ctx = (Context*)(kstack_top - sizeof(Context));
    ctx->rip = entryPoint;
    ctx->cs = 0x23;
    ctx->rflags = 0x202;
    ctx->rsp = ustack_top;
    ctx->ss = 0x1B;
    ctx->rax = ctx->rbx = ctx->rcx = ctx->rdx = 0;
    ctx->rsi = 0;
    ctx->rdi = (uint64_t)arg;
    ctx->rbp = 0;
    ctx->r8 = ctx->r9 = ctx->r10 = ctx->r11 = 0;
    ctx->r12 = ctx->r13 = ctx->r14 = ctx->r15 = 0;
    ctx->interrupt_number = 0;
    ctx->error_code = 0;

    newTask->rsp = (uint64_t)ctx;
    newTask->state = TASK_READY;
    
    spinlock_unlock_irqrestore(&g_kernel_lock, flags);
    kPrintf("Created ELF User Task at slot %d, Entry: %p\n", slot, (void*)entryPoint);
    return newTask;
}

Task* GetCurrentTask() {
    uint32_t cpu_id = get_cpu_id();
    int idx = per_cpu_task_idx[cpu_id];
    if (idx < 0 || idx >= task_count) return NULL;
    return tasks[idx];
}

void ExitCurrentTask() {
    uint64_t flags = SaveAndDisableInterrupts();
    uint32_t cpu_id = get_cpu_id();
    int idx = per_cpu_task_idx[cpu_id];
    if (idx >= 0 && tasks[idx])
        tasks[idx]->state = TASK_DEAD;
    RestoreInterrupts(flags);
    Yield();
    while(1);
}

void WaitTask(uint64_t id) {
    while (1) {
        bool found = false;
        uint64_t flags = SaveAndDisableInterrupts();
        for (int i = 0; i < task_count; i++) 
        {
            if (tasks[i] && tasks[i]->id == id) 
            {
                if (tasks[i]->state == TASK_DEAD) 
                {
                    RestoreInterrupts(flags);
                    return; // 태스크 종료됨
                }
                found = true;
                break;
            }
        }
        RestoreInterrupts(flags);
        
        if (!found) 
        {
            return; // 태스크가 아예 없음
        }
        
        Yield(); // 아직 실행 중이면 양보
    }
}

uint64_t Schedule(uint64_t current_rsp) {
    /*
     * Schedule()은 인터럽트 컨텍스트(IF=0)에서만 호출됩니다.
     * 스핀락으로 다른 CPU의 동시 접근을 막습니다.
     */
    spinlock_lock(&g_kernel_lock);

    uint32_t cpu_id = get_cpu_id();
    int my_task_idx = per_cpu_task_idx[cpu_id];

    /* 시각적 피드백: CPU별로 다른 x 위치에 스피너 표시 */
    static int sched_counts[SMP_MAX_CPUS];
    char spin[] = {'|', '/', '-', '\\'};
    int spinner_x = (int)boot_info_global->horizontal_resolution - 16 - (int)(cpu_id * 16);
    if (spinner_x > 0)
        kPutChar(boot_info_global, spinner_x, 0, spin[(sched_counts[cpu_id]++ / 10) % 4], 0x00FF0000, 0x00000033);

    if (task_count <= 1) {
        spinlock_unlock(&g_kernel_lock);
        return current_rsp;
    }

    /* 현재 태스크 RSP 저장 + READY 상태로 전환 */
    if (my_task_idx >= 0 && tasks[my_task_idx]) {
        tasks[my_task_idx]->rsp = current_rsp;
        if (tasks[my_task_idx]->state == TASK_RUNNING)
            tasks[my_task_idx]->state = TASK_READY;
    }

    /* 다음 READY 태스크 탐색 (라운드 로빈) */
    int start = (my_task_idx >= 0) ? (my_task_idx + 1) % task_count : 0;
    int next_idx = -1;
    for (int i = 0; i < task_count; i++) {
        int idx = (start + i) % task_count;
        if (tasks[idx] && tasks[idx]->state == TASK_READY) {
            next_idx = idx;
            break;
        }
    }

    if (next_idx < 0) {
        /* READY 태스크 없음: 현재 태스크를 다시 RUNNING으로 */
        if (my_task_idx >= 0 && tasks[my_task_idx] &&
            tasks[my_task_idx]->state != TASK_DEAD)
        {
            tasks[my_task_idx]->state = TASK_RUNNING;
        }
        else
        {
            /* 현재 태스크가 DEAD이거나 유효하지 않다면, per_cpu_task_idx를 -1로 리셋하여
             * 다음번 스케줄링 시 새로 로드될 태스크의 컨텍스트를 덮어쓰지 않도록 합니다. */
            per_cpu_task_idx[cpu_id] = -1;
        }
        spinlock_unlock(&g_kernel_lock);
        return current_rsp;
    }

    /* 다음 태스크를 RUNNING으로 전환 */
    tasks[next_idx]->state = TASK_RUNNING;
    per_cpu_task_idx[cpu_id] = next_idx;
    Task* next_task = tasks[next_idx];

    /* 페이지 테이블(CR3) 전환 */
    if (next_task->pml4 != NULL && next_task->pml4 != GetCR3())
        LoadPageTable(next_task->pml4);

    /* TSS RSP0 갱신 */
    uint64_t kstack_top = next_task->kernel_stack_top;
    current_kernel_stack_top_array[cpu_id] = kstack_top;
    if (kstack_top != 0) 
    {
        /* 각 CPU의 인덱스를 구해서 해당하는 TSS의 rsp0 업데이트 */
        uint8_t target_cpu_idx = 0;
        for (int i = 0; i < SMP_MAX_CPUS; i++) 
        {
            if (cpu_info[i].lapic_id == (uint8_t)cpu_id) 
            {
                target_cpu_idx = cpu_info[i].cpu_index;
                break;
            }
        }
        SetTSSStack(target_cpu_idx, kstack_top);
    }

    /* TASK_DEAD 자원 격리 로직 */
    Task* tasks_to_reclaim[MAX_TASKS];
    int reclaim_count = 0;

    for (int i = 0; i < task_count; i++)
    {
        if (tasks[i] && tasks[i]->state == TASK_DEAD)
        {
            /* 현재 어떤 CPU에서도 이 태스크를 실행 중(per_cpu_task_idx)이 아닌지 검사 */
            bool in_use = false;
            for (int cpu = 0; cpu < SMP_MAX_CPUS; cpu++)
            {
                if (per_cpu_task_idx[cpu] == i)
                {
                    in_use = true;
                    break;
                }
            }

            if (!in_use)
            {
                tasks_to_reclaim[reclaim_count++] = tasks[i];
                tasks[i] = NULL;
            }
        }
    }

    uint64_t next_rsp = next_task->rsp;
    spinlock_unlock(&g_kernel_lock);

    /* 스핀락 해제 후 안전한 락 외부 공간에서 자원 정리 수행 (데드락 방지) */
    for (int i = 0; i < reclaim_count; i++)
    {
        Task* dead_task = tasks_to_reclaim[i];
        if (dead_task)
        {
            /* 1. 페이지 디렉토리(PML4) 및 매핑된 사용자 영역 페이지 전체 해제 */
            if (dead_task->pml4)
            {
                VMM_FreeAddressSpace(dead_task->pml4);
            }

            /* 2. 커널 스택 메모리 해제 */
            if (dead_task->stack_base)
            {
                kfree(dead_task->stack_base);
            }

            /* 3. 태스크 구조체 해제 */
            kfree(dead_task);
        }
    }

    return next_rsp;
}

void Yield() {
    // 자발적 양보를 위해 타이머 인터럽트(32번)를 소프트웨어적으로 발생시킴
    // 또는 별도의 스케줄링 진입점을 사용할 수 있으나, 현재는 인터럽트 프레임 구조를 활용하기 위해 int $32 사용
    __asm__ volatile("int $32");
}
