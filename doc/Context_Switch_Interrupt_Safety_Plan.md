# Context Switching Interrupt Safety Plan (컨텍스트 스위칭 인터럽트 안전성 계획)

## 1. 개요 (Overview)
컨텍스트 스위칭(Context Switching)은 프로세서의 상태를 저장하고 복원하는 민감한 작업입니다. 이 과정 중에 인터럽트가 발생하면 커널 스택의 혼란이나 스케줄러 데이터의 오염이 발생할 수 있으므로, 원자성(Atomicity)을 보장해야 합니다.

## 2. 현재 상태 분석 (Current Status)
- **Interrupt Gate 사용**: 현재 IDT 설정은 Interrupt Gate를 사용하며, 이는 인터럽트 진입 시 CPU가 자동으로 인터럽트를 비활성화(CLI)함을 의미합니다.
- **IRETQ 복원**: 인터럽트 종료 시 `iretq` 명령어가 스택의 `RFLAGS`를 복원하며 인터럽트를 다시 활성화(STI)합니다.
- **잠재적 위험**: 
    - 향후 추가될 `yield()`, `sleep()` 등 자발적 문맥 전환 시 인터럽트 비활성화가 누락될 수 있음.
    - 스케줄러 전역 변수(`tasks`, `current_task_index`)에 대한 동시 접근 보호 필요.

## 3. 개선 방향 (Proposed Improvements)

### 3.1. 스케줄러 원자성 보장
스케줄러가 전역 상태를 수정하는 동안에는 반드시 인터럽트가 비활성화되어 있어야 합니다. 인터럽트 핸들러 내부에서 호출될 때는 이미 비활성화 상태이지만, 일반 커널 코드에서 호출될 경우를 대비해 명시적인 보호 장치를 추가합니다.

### 3.2. 자발적 문맥 전환(Yield) 구현 가이드
태스크가 스스로 CPU를 양보할 때의 절차를 정의합니다:
1. 인터럽트 비활성화 (`cli`)
2. 현재 레지스터 상태를 컨텍스트 구조체 형식으로 스택에 저장
3. `Schedule` 호출
4. 새로운 `RSP`로 스위칭
5. 인터럽트 상태 복원 및 실행 재개

### 3.3. 인터럽트 상태 저장 및 복원 유틸리티
단순히 `cli`/`sti`를 사용하는 대신, 이전 인터럽트 상태를 저장했다가 복원하는 방식을 권장합니다.
```c
long flags = SaveAndDisableInterrupts();
// Critical Section
RestoreInterrupts(flags);
```

## 4. 구현 단계 (Implementation Steps)
1. `asm_utils.asm`에 인터럽트 상태 읽기/쓰기 함수 추가.
2. `task.c`의 `Schedule` 함수 진입/퇴출 시 안전 장치 검토.
3. (선택 사항) 커널 임계 구역을 위한 스핀락(Spinlock) 기초 구조 검토.
