#include "task.h"
#include "kernel.h"
#include "heap.h"
#include "gdt.h"
#include "vmm.h"
#include "pmm.h"
#include <stdio.h>
#include <stddef.h>

extern BootInfo *boot_info_global;

static Task* tasks[MAX_TASKS];
static int task_count = 0;
static int current_task_index = 0;

uint64_t current_kernel_stack_top = 0;

void InitializeTaskSystem() {
    // 메인 커널 흐름을 0번 태스크로 등록
    Task* mainTask = (Task*)kmalloc(sizeof(Task));
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
    
    // 초기 커널 스택 상단 설정 (메인 태스크는 일단 0으로 두거나 현재 RSP 근처로 설정 가능)
    // 여기서는 일단 0으로 두고, CreateTask로 생성된 태스크부터 본격 관리
    current_kernel_stack_top = 0;
    
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
    newTask->id = task_count;
    newTask->state = TASK_READY;
    newTask->pml4 = tasks[0]->pml4; // 커널 태스크는 기본적으로 커널 페이지 테이블 공유
    
    // 태스크 스택 할당
    void* stack = kmalloc(TASK_STACK_SIZE);
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
    newTask->id = task_count;
    newTask->state = TASK_READY;
    
    /* 1. 독립된 프로세스 주소 공간(PML4) 생성 */
    newTask->pml4 = VMM_CreateAddressSpace();

    /* 2. 커널 스택 할당 (인터럽트/시스템 콜 시 복귀용) */
    void* kstack = kmalloc(TASK_STACK_SIZE);
    uint64_t kstack_top = (uint64_t)kstack + TASK_STACK_SIZE;
    newTask->kernel_stack_top = kstack_top;
    
    /* 3. 유저 스택 할당 및 매핑 (태스크 전용 PML4에) */
    void* ustack_phys = PMM_AllocPage();
    void* ustack_virt = (void*)0x00007FFFFFFF0000; // 안전한 캐노니컬 가상 주소
    VMM_MapPageEx(newTask->pml4, ustack_virt, ustack_phys, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
    uint64_t ustack_top = (uint64_t)ustack_virt + PAGE_SIZE;

    /* 4. 유저 코드 영역 권한 업데이트 (PAGE_USER 추가) */
    /* 현재는 커널 내부 함수를 사용하므로 해당 페이지를 찾아 권한만 확장합니다. 
     * 함수가 페이지 경계에 걸쳐 있을 수 있으므로 2개 페이지 정도를 넉넉히 매핑합니다. */
    uint64_t code_page = (uint64_t)entryPoint & ~0xFFFULL;
    VMM_MapPageEx(newTask->pml4, (void*)code_page, (void*)code_page, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
    VMM_MapPageEx(newTask->pml4, (void*)(code_page + PAGE_SIZE), (void*)(code_page + PAGE_SIZE), PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);

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
    tasks[task_count++] = newTask;
    
    RestoreInterrupts(flags);
    printf("Created Isolated User Task with arg: %d, PML4: %p\n", arg, newTask->pml4);
    
    return newTask;
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
    do {
        current_task_index = (current_task_index + 1) % task_count;
    } while (tasks[current_task_index]->state == TASK_SLEEP);

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
