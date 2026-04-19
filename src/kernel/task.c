#include "task.h"
#include "kernel.h"
#include "heap.h"
#include <stddef.h>

extern BootInfo *boot_info_global;

static Task* tasks[MAX_TASKS];
static int task_count = 0;
static int current_task_index = 0;

void InitializeTaskSystem() {
    // 메인 커널 흐름을 0번 태스크로 등록
    Task* mainTask = (Task*)kmalloc(sizeof(Task));
    mainTask->id = task_count++;
    mainTask->state = TASK_RUNNING;
    mainTask->stack_base = NULL; // 이미 설정된 커널 스택 사용
    mainTask->rsp = 0;           // Schedule 최초 호출 시 저장됨
    
    for (int i = 0; i < MAX_TASKS; i++) {
        tasks[i] = NULL;
    }
    
    tasks[0] = mainTask;
    current_task_index = 0;
    
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

uint64_t Schedule(uint64_t current_rsp) {
    // 시각적 피드백: 스케줄러가 호출될 때마다 화면 왼쪽 상단에 'S' 출력
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

    // 새로운 태스크의 RSP 반환
    return tasks[current_task_index]->rsp;
}
