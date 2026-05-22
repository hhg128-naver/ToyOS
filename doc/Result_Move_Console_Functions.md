# kernel.c 정리 및 콘솔 기능 분리 완료 보고서

## 1. 작업 개요
`src/kernel/kernel.c` 파일에 혼재되어 있던 그래픽 콘솔 제어 로직을 별도의 모듈로 분리하여 코드 가독성을 높이고 역할을 명확히 하였습니다.

## 2. 주요 변경 사항

### 2.1 신규 파일 생성
- **`src/kernel/console.h`**: 콘솔 관련 함수(`Printf`, `ClearScreen`, `PutChar`, `PrintString`) 및 전역 변수(`boot_info_global`)의 외부 선언을 포함합니다.
- **`src/kernel/console.c`**: `kernel.c`에서 이동된 실제 그래픽 콘솔 구현 로직을 담고 있습니다. 커서 위치 관리 및 화면 스크롤 기능이 포함됩니다.

### 2.2 기존 파일 수정
- **`src/kernel/kernel.c`**:
    - 약 150라인 이상의 콘솔 관련 코드를 제거하였습니다.
    - `kmain`에서는 `Console_Init(boot_info)`를 호출하여 콘솔 시스템을 초기화합니다.
    - 이로써 `kernel.c`는 GDT, IDT, 메모리, 태스크 시스템 초기화 등 커널의 핵심 엔트리 역할에만 집중하게 되었습니다.
- **`src/kernel/kernel.h`**:
    - 중복된 그래픽 콘솔 선언을 제거하고 `console.h`를 인클루드하도록 수정하여 하위 호환성을 유지했습니다.
- **`src/kernel/shell.cpp`**:
    - `ClearScreen`과 `boot_info_global`을 사용하기 위해 내부에서 수동으로 하던 `extern` 선언을 제거하고 `console.h`를 참조하도록 개선했습니다.
- **`Makefile`**:
    - `console.c`가 자동으로 빌드 및 링크 프로세스에 포함되도록 확인하였습니다.

## 3. 결과 확인
- `make all` 명령을 통해 전체 프로젝트가 에러 없이 빌드됨을 확인하였습니다.
- 커널 초기화 과정 및 쉘의 `clear` 명령어 등이 기존과 동일하게 정상 작동할 수 있는 구조임을 검증하였습니다.

## 4. 향후 계획
- 콘솔 모듈을 기반으로 가상 터미널(VT100 등) 지원이나 더 복잡한 텍스트 렌더링 기능을 추가할 수 있는 기반이 마련되었습니다.
