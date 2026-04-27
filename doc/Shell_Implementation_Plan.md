# 간단한 쉘 구현 계획 (Shell Implementation Plan)

ToyOS에서 사용자와 상호작용할 수 있는 기초적인 쉘(Shell)을 구현합니다. 이를 위해 키보드 입력 시스템을 개선하고, Newlib의 표준 입력을 활성화합니다.

## User Review Required
> [!IMPORTANT]
> 쉘은 커널 모드 태스크로 실행될 예정입니다. 나중에 ELF 로더가 구현되면 사용자 모드 프로세스로 전환될 수 있습니다.

## Proposed Changes

### 1. 키보드 입력 시스템 개선
키보드 인터럽트 시 문자를 즉시 화면에 출력하는 대신, 내부 버퍼에 저장하고 필요한 시점에 꺼내 쓸 수 있도록 합니다.

#### [MODIFY] [keyboard.h](file:///f:/ToyOS/src/kernel/keyboard.h)
- 키보드 버퍼 사이즈 정의
- `Keyboard_GetChar()` 함수 선언

#### [MODIFY] [keyboard.c](file:///f:/ToyOS/src/kernel/keyboard.c)
- 원형 버퍼(Circular Buffer) 구현
- `Keyboard_Handler`: 스캔코드를 문자로 변환하여 버퍼에 삽입.
- `Keyboard_GetChar`: 버퍼에서 문자를 하나 가져옴. 버퍼가 비어있으면 `Yield()`를 호출하며 대기.

### 2. Newlib 시스템 콜 연동
`printf` 뿐만 아니라 `scanf`, `fgets` 등을 사용할 수 있도록 `read` 시스템 콜을 구현합니다.

#### [MODIFY] [syscalls.c](file:///f:/ToyOS/src/kernel/syscalls.c)
- `read` 함수 수정: `file == 0` (stdin)일 때 `Keyboard_GetChar()`를 사용하여 데이터를 채우도록 함.

### 3. 쉘(Shell) 구현
독립적인 태스크로 실행될 쉘 로직을 작성합니다.

#### [NEW] [shell.h](file:///f:/ToyOS/src/kernel/shell.h)
- `Shell_Main()` 함수 선언

#### [NEW] [shell.c](file:///f:/ToyOS/src/kernel/shell.c)
- `Shell_Main`: "ToyOS> " 프롬프트 출력 및 사용자 입력 대기
- 명령어 파싱 및 실행 (help, clear, mem, exit 등)

### 4. 커널 초기화 및 통합

#### [MODIFY] [kernel.c](file:///f:/ToyOS/src/kernel/kernel.c)
- `kmain`: 쉘 태스크 생성 및 실행

## Verification Plan

### Automated Tests
- `make run-uefi` 명령어로 QEMU 실행
- 키보드 입력이 화면에 올바르게 표시되는지 확인
- `help` 명령어 입력 시 도움말 출력 확인
- `clear` 명령어 입력 시 화면 초기화 확인

### Manual Verification
- 백스페이스 작동 여부 확인
- 여러 글자 입력 후 엔터 키로 명령어가 정상 실행되는지 확인
