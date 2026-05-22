# 콘솔 관련 기능 분리 및 kernel.c 정리 계획

## 1. 개요
현재 `src/kernel/kernel.c` 파일에는 커널의 초기화 로직뿐만 아니라 그래픽 콘솔 제어(문자 출력, 화면 초기화, 스크롤 등)를 위한 로직이 포함되어 있습니다. 이러한 출력 관련 로직은 쉘(Shell)이나 다른 시스템 구성 요소에서 주로 사용되므로, 이를 별도의 `console.c`와 `console.h`로 분리하여 `kernel.c`를 핵심 초기화 로직에 집중하도록 리팩토링합니다.

## 2. 분석
`kernel.c`에서 분리할 대상:
- 전역 변수: `cursor_x`, `cursor_y`, `boot_info_global`, `current_bg_color`
- 함수: `Printf()`, `ClearScreen()`, `PutChar()`, `ScrollUp()`, `PrintString()`

이 기능들은 저수준 그래픽 출력(GOP 프레임버퍼 조작)을 담당하며, 쉘의 터미널 기능과 밀접한 관련이 있습니다.

## 3. 변경 계획

### 3.1 새로운 파일 생성
- `src/kernel/console.h`: 콘솔 관련 함수 및 구조체 선언.
- `src/kernel/console.c`: `kernel.c`에서 추출한 콘솔 구현 로직.

### 3.2 kernel.c 수정
- 콘솔 관련 전역 변수 및 함수 제거.
- `kmain()`에서 `console_init()` (가칭)을 호출하도록 변경.
- `#include "console.h"` 추가.

### 3.3 kernel.h 수정
- 그래픽 콘솔 관련 선언을 `console.h`로 이동시키거나, `console.h`를 포함하도록 수정.

### 3.4 shell.cpp 수정
- `ClearScreen` 등을 사용할 때 `console.h`를 참조하도록 수정.

### 3.5 Makefile 수정
- 빌드 대상에 `console.o` 추가.

## 4. 기대 효과
- `kernel.c`의 복잡도 감소 (초기화 흐름 파악 용이).
- 콘솔 로직의 모듈화로 향후 터미널 에뮬레이션 기능 확장 용이.
- 쉘과 콘솔 간의 논리적 연결성 강화.
