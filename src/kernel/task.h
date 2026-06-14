#ifndef TASK_H
#define TASK_H

#include <stdint.h>

#define TASK_STACK_SIZE 8192 // 8KB stack per task
#define MAX_TASKS 32

typedef enum {
    TASK_READY,
    TASK_RUNNING,
    TASK_SLEEP,
    TASK_DEAD
} TaskState;

// irq_common에서의 푸시 순서에 맞춘 컨텍스트 구조체
typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rdx, rcx, rbx, rax;
    uint64_t interrupt_number, error_code;
    uint64_t rip, cs, rflags, rsp, ss;
} Context;

typedef struct Task {
    uint64_t rsp;          // 현재 스택 포인터 (Context 구조체의 주소)
    uint64_t id;           // 태스크 식별자
    TaskState state;       // 태스크 상태
    void* stack_base;      // 할당된 스택 메모리 주소
    uint64_t kernel_stack_top; // 커널 스택 최상단 주소
    void* pml4;            // 태스크전용 페이지 테이블 (주소 공간)
    uint64_t heap_start;   // 유저 힙 시작 주소
    uint64_t heap_end;     // 유저 힙 현재 끝 주소
} Task;

extern uint64_t current_kernel_stack_top_array[16];

void InitializeTaskSystem();
Task* CreateTask(void (*entryPoint)());
Task* CreateUserTask(void (*entryPoint)(), int arg);
Task* CreateELFTask(uint64_t entryPoint, int arg, void* pml4);
void ExitCurrentTask();
void WaitTask(uint64_t id);
Task* GetCurrentTask();
uint64_t Schedule(uint64_t current_rsp);
void Yield();

#endif
