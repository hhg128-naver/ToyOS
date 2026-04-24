# 유저 프로세스 격리 작업 계획 (User Process Isolation Plan)

ToyOS의 각 유저 프로세스가 독립된 가상 주소 공간을 가질 수 있도록 페이지 테이블(PML4)을 분격적으로 격리하는 작업을 수행합니다.

## 목표
- 각 태스크(Task)마다 독립적인 PML4 페이지 테이블 할당.
- 컨텍스트 스위칭(Schedule) 시 CR3 레지스터 전환을 통한 주소 공간 격리.
- 커널 영역(0~1GB, Framebuffer 등)은 모든 프로세스가 공유하도록 설정.

## 상세 단계

### 1. 가상 메모리 관리자(VMM) 개선
- `VMM_MapPage` 함수가 특정 PML4를 인자로 받도록 수정 (`VMM_MapPageEx`).
- 새로운 프로세스를 위한 주소 공간 생성 함수 `VMM_CreateAddressSpace()` 추가.
    - 새로운 PML4 페이지 할당 및 초기화.
    - 마스터 커널 페이지 테이블(`kernel_pml4`)의 내용을 복사하여 커널 영역 공유.

### 2. 태스크 관리 시스템(Task System) 수정
- `Task` 구조체에 `void* pml4` 필드 추가.
- `CreateUserTask` 호출 시 `VMM_CreateAddressSpace()`를 통해 독립된 페이지 테이블 생성.
- 유저 스택 등의 매핑 시 해당 태스크 전용 PML4를 사용하도록 수정.

### 3. 스케줄러(Scheduler) 수정
- 태스크 전환 시 `next_task->pml4`가 존재한다면 `LoadPageTable()`을 호출하여 CR3 레지스터 갱신.

### 4. 검증
- 서로 다른 유저 태스크가 동일한 가상 주소(예: 유저 스택 주소)를 사용하더라도 물리적으로 격리되어 충돌이 발생하지 않는지 확인.

## 파일별 변경 사항

### [kernel/vmm.h](file:///f:/ToyOS/src/kernel/vmm.h)
- `VMM_MapPageEx` 함수 선언 추가.
- `VMM_CreateAddressSpace` 함수 선언 추가.

### [kernel/vmm.c](file:///f:/ToyOS/src/kernel/vmm.c)
- `VMM_MapPageEx` 구현 (PML4를 인자로 받음).
- `VMM_CreateAddressSpace` 구현.

### [kernel/task.h](file:///f:/ToyOS/src/kernel/task.h)
- `struct Task`에 `void* pml4` 추가.

### [kernel/task.c](file:///f:/ToyOS/src/kernel/task.c)
- `CreateUserTask`에서 전용 PML4 생성.
- `Schedule`에서 `LoadPageTable` 호출부 추가.
