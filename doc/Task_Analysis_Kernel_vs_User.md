# ToyOS 태스크 분석: 커널 태스크 vs 유저 태스크

이 문서는 `src/kernel/task.c`의 소스 코드를 바탕으로 커널 모드 태스크와 유저 모드 태스크의 구현 차이점 및 동작 원리를 분석합니다.

## 1. 개요 (Overview)
ToyOS는 x86_64 아키텍처에서 멀티태스킹을 지원하며, 태스크는 크게 커널 권한(Ring 0)에서 실행되는 **커널 태스크**와 유저 권한(Ring 3)에서 실행되는 **유저 태스크**로 나뉩니다.

## 2. 주요 차이점 요약 (Summary of Differences)

| 항목 | 커널 태스크 (`CreateTask`) | 유저 태스크 (`CreateUserTask`) |
| :--- | :--- | :--- |
| **권한 레벨 (DPL/RPL)** | Ring 0 (Highest) | Ring 3 (Lowest) |
| **코드 세그먼트 (CS)** | `0x08` (Kernel Code) | `0x23` (User Code, RPL 3) |
| **데이터 세그먼트 (SS)** | `0x10` (Kernel Data) | `0x1B` (User Data, RPL 3) |
| **스택 구조** | 단일 스택 (Kernel Stack) | 이중 스택 (User Stack + Kernel Stack) |
| **메모리 접근 권한** | 커널 영역 직접 접근 가능 | `PAGE_USER` 플래그가 설정된 페이지만 접근 가능 |

## 3. 상세 분석 (Detailed Analysis)

### 3.1. 세그먼트 및 권한 설정
- **커널 태스크**: GDT의 커널 코드/데이터 세그먼트를 사용하며, RPL(Requested Privilege Level)이 0입니다. CPU는 이를 통해 모든 특권 명령어를 실행할 수 있는 상태가 됩니다.
- **유저 태스크**: 유저 코드/데이터 세그먼트를 사용하며, 셀렉터 하위 2비트가 `3`으로 설정되어 있습니다. 이는 `IRETQ` 명령어를 통해 유저 모드로 진입할 때 CPU의 권한 레벨을 Ring 3로 전환시킵니다.

### 3.2. 스택 관리 (Stack Management)
- **커널 태스크**:
    - `kmalloc`으로 할당된 하나의 스택 공간에서 태스크의 로직 실행과 인터럽트 처리가 모두 이루어집니다.
    - `ctx->rsp`는 이 할당된 스택의 최상단을 가리킵니다.
- **유저 태스크**:
    - **유저 스택 (User Stack)**: 유저 모드에서 일반적인 연산을 수행할 때 사용됩니다. `PMM_AllocPage`를 통해 할당되며 가상 주소 `0x00007FFFFFFF0000` 부근에 매핑됩니다.
    - **커널 스택 (Kernel Stack)**: 유저 모드에서 인터럽트나 시스템 콜이 발생했을 때, CPU가 안전하게 컨텍스트를 저장하기 위해 전환되는 공간입니다.
    - **TSS 연동**: `Schedule` 함수에서 유저 태스크로 전환될 때 `SetTSSStack`을 호출하여, 하드웨어가 Ring 3 -> Ring 0 전환 시 사용할 커널 스택 주소를 TSS(Task State Segment)에 등록합니다.

### 3.3. 페이지 테이블 및 보안 (Memory Protection)
- 유저 태스크가 실행되려면 해당 태스크가 사용하는 코드 영역과 스택 영역의 페이지 테이블 엔트리에 `PAGE_USER` 비트가 설정되어야 합니다.
- `CreateUserTask`에서는 `VMM_MapPage`를 호출하여 유저가 접근할 수 있도록 권한을 확장하는 과정이 포함되어 있습니다.

## 4. 스케줄링 및 컨텍스트 스위칭
`Schedule` 함수는 태스크의 종류에 관계없이 동일한 `Context` 구조체를 사용하여 레지스터 정보를 저장하고 복원합니다. 하지만 유저 태스크의 경우 `current_kernel_stack_top` 변수를 업데이트함으로써, 다음 인터럽트 발생 시 올바른 커널 스택으로 복귀할 수 있도록 보장합니다.
