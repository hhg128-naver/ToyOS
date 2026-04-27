# 쉘 구현 완료 보고서 (Shell Implementation Walkthrough)

ToyOS에서 사용자와 상호작용할 수 있는 기초적인 쉘(Shell)을 성공적으로 구현하였습니다.

## 주요 변경 사항

### 1. 키보드 입력 시스템 개선
- **버퍼 도입**: `keyboard.c`에 원형 버퍼(Circular Buffer)를 도입하여 인터럽트 발생 시 입력을 저장하도록 했습니다.
- **`Keyboard_GetChar()`**: 프로세스가 입력을 요청할 때 버퍼가 비어있으면 `Yield()`를 통해 CPU를 양보하며 대기하는 메커니즘을 구현했습니다.

### 2. 표준 입력(Standard Input) 연동
- **Newlib `read` 구현**: `syscalls.c`의 `read` 시스템 콜이 `Keyboard_GetChar()`를 사용하도록 수정했습니다. 이로써 `scanf`, `fgets`, `getchar` 등 표준 C 라이브러리 함수를 쉘에서 사용할 수 있게 되었습니다.

### 3. 쉘(Shell) 태스크
- **명령어 처리 루프**: `shell.c`에 `Shell_Main` 함수를 구현하여 "ToyOS> " 프롬프트를 출력하고 사용자의 입력을 처리합니다.
- **지원 명령어**:
    - `help`: 사용 가능한 명령어 목록 출력
    - `clear`: 화면 초기화
    - `mem`: 물리 메모리 상태(전체, 사용 중, 여유 공간) 출력
    - `reboot`: (미구현 안내)

### 4. 시스템 통합
- **쉘 태스크 실행**: `kernel.c`의 `kmain` 마지막 부분에서 `CreateTask(Shell_Main)`를 호출하여 시스템 부팅과 동시에 쉘이 시작되도록 설정했습니다.

## 테스트 결과
- QEMU 환경에서 빌드 및 실행 확인.
- 키보드 입력 및 에코(Echo) 정상 작동.
- `help`, `clear`, `mem` 명령어 정상 작동 확인.

## 향후 작업
- 가상 파일 시스템(VFS) 연동 후 `ls`, `cd`, `cat` 등의 명령어 추가.
- ELF 로더 구현 후 사용자 모드 프로그램 실행 지원.
