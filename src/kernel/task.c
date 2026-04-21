#include "task.h"
#include "kernel.h"
#include "heap.h"
#include "gdt.h"
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
    if (task_count >= MAX_TASKS) {
        Printf("Error: Max tasks reached.\n");
        return NULL;
    }

    Task* newTask = (Task*)kmalloc(sizeof(Task));
    newTask->id = task_count;
    newTask->state = TASK_READY;
    
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
    
    Printf("Created New Task.\n");
    
    return newTask;
}

Task* CreateUserTask(void (*entryPoint)()) {
    if (task_count >= MAX_TASKS) {
        Printf("Error: Max tasks reached.\n");
        return NULL;
    }

    Task* newTask = (Task*)kmalloc(sizeof(Task));
    newTask->id = task_count;
    newTask->state = TASK_READY;
    
    /* 1. 커널 스택 할당 (인터럽트/시스템 콜 시 복귀용) */
    void* kstack = kmalloc(TASK_STACK_SIZE);
    uint64_t kstack_top = (uint64_t)kstack + TASK_STACK_SIZE;
    newTask->kernel_stack_top = kstack_top;
    
    /* 2. 유저 스택 할당 및 매핑 */
    void* ustack_phys = PMM_AllocPage();
    void* ustack_virt = (void*)0x00007FFFFFFF0000; // 안전한 캐노니컬 가상 주소
    VMM_MapPage(ustack_virt, ustack_phys, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
    uint64_t ustack_top = (uint64_t)ustack_virt + PAGE_SIZE;

    /* 3. 유저 코드 영역 권한 업데이트 (PAGE_USER 추가) */
    /* 현재는 커널 내부 함수를 사용하므로 해당 페이지를 찾아 권한만 확장합니다. 
     * 함수가 페이지 경계에 걸쳐 있을 수 있으므로 2개 페이지 정도를 넉넉히 매핑합니다. */
    uint64_t code_page = (uint64_t)entryPoint & ~0xFFFULL;
    VMM_MapPage((void*)code_page, (void*)code_page, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
    VMM_MapPage((void*)(code_page + PAGE_SIZE), (void*)(code_page + PAGE_SIZE), PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);

    /* 4. 초기 컨텍스트 설정 (커널 스택에 저장) */
    Context* ctx = (Context*)(kstack_top - sizeof(Context));
    
    ctx->rip = (uint64_t)entryPoint;
    ctx->cs = 0x23;         // User Code Segment (Index 4, RPL 3)
    ctx->rflags = 0x202;    // IF=1
    ctx->rsp = ustack_top;  // 유저 스택 포인터
    ctx->ss = 0x1B;         // User Data Segment (Index 3, RPL 3)
    
    // 범용 레지스터 초기화
    ctx->rax = ctx->rbx = ctx->rcx = ctx->rdx = 0;
    ctx->rsi = ctx->rdi = ctx->rbp = 0;
    ctx->r8 = ctx->r9 = ctx->r10 = ctx->r11 = 0;
    ctx->r12 = ctx->r13 = ctx->r14 = ctx->r15 = 0;
    ctx->interrupt_number = 0;
    ctx->error_code = 0;

    newTask->rsp = (uint64_t)ctx;
    tasks[task_count++] = newTask;
    
    Printf("Created New User Task.\n");
    
    return newTask;
}

uint64_t Schedule(uint64_t current_rsp) {
    // 시각적 피드백
    static int sched_count = 0;
    char spin[] = {'|', '/', '-', '\\'};
    PutChar(boot_info_global, boot_info_global->horizontal_resolution - 16, 0, spin[(sched_count++ / 10) % 4], 0x00FF0000, 0x00000033);

    if (task_count <= 1) return current_rsp;

    // 현재 실행 중인 태스크의 RSP 저장
    tasks[current_task_index]->rsp = current_rsp;

    // 다음 READY 상태 태스크 선택 (라운드 로빈)
    do {
        current_task_index = (current_task_index + 1) % task_count;
    } while (tasks[current_task_index]->state == TASK_SLEEP);

    // 새로운 태스크의 정보를 전역 변수 및 TSS에 반영
    current_kernel_stack_top = tasks[current_task_index]->kernel_stack_top;
    if (current_kernel_stack_top != 0) {
        SetTSSStack(current_kernel_stack_top);
    }

    // 새로운 태스크의 RSP 반환
    return tasks[current_task_index]->rsp;
}
