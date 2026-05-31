#include "task.h"
#include "kernel.h"
#include "heap.h"
#include "gdt.h"
#include "vmm.h"
#include "pmm.h"
#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>

extern BootInfo *boot_info_global;

static Task* tasks[MAX_TASKS];
static int task_count = 0;
static int current_task_index = 0;

uint64_t current_kernel_stack_top = 0;

void InitializeTaskSystem() {
    // 메인 커널 흐름을 0번 태스크로 등록
    Task* mainTask = (Task*)kmalloc(sizeof(Task));
    if (!mainTask) return;
    
    mainTask->id = task_count++;
    mainTask->state = TASK_RUNNING;
    mainTask->stack_base = NULL; // 이미 설정된 커널 스택 사용
    mainTask->kernel_stack_top = 0; // 메인 커널 스택은 별도 관리됨 (보통 부트 시 설정)
    mainTask->rsp = 0;           // Schedule 최초 호출 시 저장됨
    mainTask->pml4 = GetCR3();    // 현재 로드된 커널 페이지 테이블 저장
    
    for (int i = 0; i < MAX_TASKS; i++) {
        tasks[i] = NULL;
    }
    
    tasks[0] = mainTask;
    current_task_index = 0;
    
    // 초기 커널 스택 상단 설정 (메인 태스크는 일단 현재 RSP 근처를 기준으로 삼음)
    uint64_t dummy_rsp;
    __asm__ volatile("mov %%rsp, %0" : "=r"(dummy_rsp));
    current_kernel_stack_top = dummy_rsp;
    
    Printf("Task System Initialized.\n");
}

Task* CreateTask(void (*entryPoint)()) {
    uint64_t flags = SaveAndDisableInterrupts();

    if (task_count >= MAX_TASKS) {
        Printf("Error: Max tasks reached.\n");
        RestoreInterrupts(flags);
        return NULL;
    }

    Task* newTask = (Task*)kmalloc(sizeof(Task));
    if (!newTask) {
        RestoreInterrupts(flags);
        return NULL;
    }
    
    // 태스크 스택 할당
    void* stack = kmalloc(TASK_STACK_SIZE);
    if (!stack) {
        kfree(newTask);
        RestoreInterrupts(flags);
        return NULL;
    }

    newTask->id = task_count;
    newTask->state = TASK_READY;
    newTask->pml4 = tasks[0]->pml4; // 커널 태스크는 기본적으로 커널 페이지 테이블 공유
    newTask->stack_base = stack;
    
    // 스택 최상단 (64비트 정렬)
    uint64_t stack_top = (uint64_t)stack + TASK_STACK_SIZE;
    newTask->kernel_stack_top = stack_top;
    
    // Context 구조체 위치 잡기
    Context* ctx = (Context*)(stack_top - sizeof(Context));
    
    // 초기 레지스터 상태 설정
    ctx->rip = (uint64_t)entryPoint;
    ctx->cs = 0x08;         // Kernel Code Segment
    ctx->rflags = 0x202;    // IF=1 (Interrupts Enabled)
    ctx->rsp = stack_top;   // IRETQ가 복원할 RSP
    ctx->ss = 0x10;         // Kernel Data Segment
    
    // 범용 레지스터 초기화
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
    
    RestoreInterrupts(flags);
    Printf("Created New Task.\n");
    
    return newTask;
}

Task* CreateUserTask(void (*entryPoint)(), int arg) {
    uint64_t flags = SaveAndDisableInterrupts();

    if (task_count >= MAX_TASKS) {
        Printf("Error: Max tasks reached.\n");
        RestoreInterrupts(flags);
        return NULL;
    }

    Task* newTask = (Task*)kmalloc(sizeof(Task));
    if (!newTask) {
        RestoreInterrupts(flags);
        return NULL;
    }
    
    /* 1. 독립된 프로세스 주소 공간(PML4) 생성 */
    void* pml4 = VMM_CreateAddressSpace();
    if (!pml4) {
        kfree(newTask);
        RestoreInterrupts(flags);
        return NULL;
    }

    /* 2. 커널 스택 할당 (인터럽트/시스템 콜 시 복귀용) */
    void* kstack = kmalloc(TASK_STACK_SIZE);
    if (!kstack) {
        VMM_FreeAddressSpace(pml4);
        kfree(newTask);
        RestoreInterrupts(flags);
        return NULL;
    }

    /* 3. 유저 스택 할당 및 매핑 (태스크 전용 PML4에) */
    void* ustack_phys = PMM_AllocPage();
    if (!ustack_phys) {
        kfree(kstack);
        VMM_FreeAddressSpace(pml4);
        kfree(newTask);
        RestoreInterrupts(flags);
        return NULL;
    }
    void* ustack_virt = (void*)0x00007FFFFFFF0000; // 안전한 캐노니컬 가상 주소
    VMM_MapPageEx(pml4, ustack_virt, ustack_phys, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
    uint64_t ustack_top = (uint64_t)ustack_virt + PAGE_SIZE;

    /* 4. 유저 코드 영역 권한 업데이트 (PAGE_USER 추가) */
    uint64_t code_page = (uint64_t)entryPoint & ~0xFFFULL;
    VMM_MapPageEx(pml4, (void*)code_page, (void*)code_page, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
    VMM_MapPageEx(pml4, (void*)(code_page + PAGE_SIZE), (void*)(code_page + PAGE_SIZE), PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);

    uint64_t kstack_top = (uint64_t)kstack + TASK_STACK_SIZE;
    newTask->id = task_count;
    newTask->state = TASK_READY;
    newTask->pml4 = pml4;
    newTask->stack_base = kstack;
    newTask->kernel_stack_top = kstack_top;
    
    /* 5. 초기 컨텍스트 설정 (커널 스택에 저장) */
    Context* ctx = (Context*)(kstack_top - sizeof(Context));
    
    ctx->rip = (uint64_t)entryPoint;
    ctx->cs = 0x23;         // User Code Segment (Index 4, RPL 3)
    ctx->rflags = 0x202;    // IF=1
    ctx->rsp = ustack_top;  // 유저 스택 포인터
    ctx->ss = 0x1B;         // User Data Segment (Index 3, RPL 3)
    
    // 범용 레지스터 초기화
    ctx->rax = ctx->rbx = ctx->rcx = ctx->rdx = 0;
    ctx->rsi = 0;
    ctx->rdi = (uint64_t)arg; // 첫 번째 인자 전달 (x86_64 calling convention)
    ctx->rbp = 0;
    ctx->r8 = ctx->r9 = ctx->r10 = ctx->r11 = 0;
    ctx->r12 = ctx->r13 = ctx->r14 = ctx->r15 = 0;
    ctx->interrupt_number = 0;
    ctx->error_code = 0;

    newTask->rsp = (uint64_t)ctx;
    newTask->heap_start = 0x60000000;
    newTask->heap_end = 0x60000000;
    tasks[task_count++] = newTask;
    
    RestoreInterrupts(flags);
    printf("Created Isolated User Task with arg: %d, PML4: %p\n", arg, newTask->pml4);
    
    return newTask;
}

Task* CreateELFTask(uint64_t entryPoint, int arg, void* pml4) {
    uint64_t flags = SaveAndDisableInterrupts();

    /* 1. 종료된 태스크 슬롯 재사용 탐색 */
    int slot = -1;
    for (int i = 0; i < MAX_TASKS; i++) 
    {
        if (tasks[i] == NULL) 
        {
            slot = i;
            break;
        } 
        // else if (tasks[i]->state == TASK_DEAD) 
        // {
        //     /* 기존 자원 해제 */
        //     VMM_FreeAddressSpace(tasks[i]->pml4);
        //     if (tasks[i]->stack_base) kfree(tasks[i]->stack_base);
        //     tasks[i]->stack_base = NULL;
        //     tasks[i]->pml4 = NULL;
        //     slot = i;
        //     break;
        // }
    }

    if (slot == -1) {
        Printf("Error: Max tasks reached.\n");
        RestoreInterrupts(flags);
        return NULL;
    }

    /* 커널 스택 할당 (인터럽트/시스템 콜 시 복귀용) */
    void* kstack = kmalloc(TASK_STACK_SIZE);
    if (!kstack) {
        RestoreInterrupts(flags);
        return NULL;
    }
    
    /* 유저 스택 할당 및 매핑 (태스크 전용 PML4에) */
    void* ustack_phys = PMM_AllocPage();
    if (!ustack_phys) {
        kfree(kstack);
        RestoreInterrupts(flags);
        return NULL;
    }
    void* ustack_virt = (void*)0x00007FFFFFFF0000; // 안전한 캐노니컬 가상 주소
    VMM_MapPageEx(pml4, ustack_virt, ustack_phys, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
    uint64_t ustack_top = (uint64_t)ustack_virt + PAGE_SIZE;

    Task* newTask = tasks[slot];
    if (newTask == NULL) {
        newTask = (Task*)kmalloc(sizeof(Task));
        if (!newTask) {
            PMM_FreePage(ustack_phys); 
            kfree(kstack);
            RestoreInterrupts(flags);
            return NULL;
        }
        tasks[slot] = newTask;
        if (slot >= task_count) task_count = slot + 1;
    }
    
    newTask->id = slot; // 슬롯 인덱스를 ID로 사용
    newTask->pml4 = pml4;
    newTask->stack_base = kstack;
    uint64_t kstack_top = (uint64_t)kstack + TASK_STACK_SIZE;
    newTask->kernel_stack_top = kstack_top;
    
    /* 초기 컨텍스트 설정 (커널 스택에 저장) */
    Context* ctx = (Context*)(kstack_top - sizeof(Context));
    
    ctx->rip = entryPoint;
    ctx->cs = 0x23;         // User Code Segment (Index 4, RPL 3)
    ctx->rflags = 0x202;    // IF=1
    ctx->rsp = ustack_top;  // 유저 스택 포인터
    ctx->ss = 0x1B;         // User Data Segment (Index 3, RPL 3)
    
    // 범용 레지스터 초기화
    ctx->rax = ctx->rbx = ctx->rcx = ctx->rdx = 0;
    ctx->rsi = 0;
    ctx->rdi = (uint64_t)arg; // 첫 번째 인자 전달
    ctx->rbp = 0;
    ctx->r8 = ctx->r9 = ctx->r10 = ctx->r11 = 0;
    ctx->r12 = ctx->r13 = ctx->r14 = ctx->r15 = 0;
    ctx->interrupt_number = 0;
    ctx->error_code = 0;

    newTask->rsp = (uint64_t)ctx;
    newTask->state = TASK_READY; // 모든 준비가 끝난 후 상태 변경
    
    RestoreInterrupts(flags);
    printf("Created ELF User Task at slot %d, Entry: %p\n", slot, (void*)entryPoint);
    
    return newTask;
}

Task* GetCurrentTask() {
    return tasks[current_task_index];
}

void ExitCurrentTask() {
    uint64_t flags = SaveAndDisableInterrupts();
    tasks[current_task_index]->state = TASK_DEAD;
    RestoreInterrupts(flags);
    Yield();
    while(1); // 절대 도달하지 않음
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
    // 스케줄러 핵심 로직 보호
    uint64_t flags = SaveAndDisableInterrupts();

    // 시각적 피드백
    static int sched_count = 0;
    char spin[] = {'|', '/', '-', '\\'};
    PutChar(boot_info_global, boot_info_global->horizontal_resolution - 16, 0, spin[(sched_count++ / 10) % 4], 0x00FF0000, 0x00000033);

    if (task_count <= 1) {
        RestoreInterrupts(flags);
        return current_rsp;
    }

    // 현재 실행 중인 태스크의 RSP 저장
    tasks[current_task_index]->rsp = current_rsp;

    // 다음 READY 상태 태스크 선택 (라운드 로빈)
    int next_index = current_task_index;
    do {
        next_index = (next_index + 1) % task_count;
    } while (next_index != current_task_index && (tasks[next_index] == NULL || tasks[next_index]->state == TASK_SLEEP || tasks[next_index]->state == TASK_DEAD));

    // 실행 가능한 태스크를 찾지 못한 경우 (자기 자신도 SLEEP/DEAD인 경우 등)
    if (tasks[next_index] == NULL || tasks[next_index]->state == TASK_SLEEP || tasks[next_index]->state == TASK_DEAD) {
        RestoreInterrupts(flags);
        return current_rsp;
    }

    current_task_index = next_index;
    Task* next_task = tasks[current_task_index];

    // 페이지 테이블(CR3) 전환 - 주소 공간 격리의 핵심
    if (next_task->pml4 != NULL && next_task->pml4 != GetCR3()) {
        LoadPageTable(next_task->pml4);
    }

    // 새로운 태스크의 정보를 전역 변수 및 TSS에 반영
    current_kernel_stack_top = next_task->kernel_stack_top;
    if (current_kernel_stack_top != 0) {
        SetTSSStack(current_kernel_stack_top);
    }

    uint64_t next_rsp = next_task->rsp;
    
    RestoreInterrupts(flags);

    return next_rsp;
}

void Yield() {
    // 자발적 양보를 위해 타이머 인터럽트(32번)를 소프트웨어적으로 발생시킴
    // 또는 별도의 스케줄링 진입점을 사용할 수 있으나, 현재는 인터럽트 프레임 구조를 활용하기 위해 int $32 사용
    __asm__ volatile("int $32");
}
