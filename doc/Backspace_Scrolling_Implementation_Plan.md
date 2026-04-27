# 백스페이스 및 화면 스크롤 구현 계획 (Backspace & Scrolling Implementation Plan)

사용자 경험 향상을 위해 쉘 환경에서 필수적인 백스페이스 처리와 화면 하단 도달 시 자동 스크롤 기능을 구현합니다.

## 제안된 변경 사항 (Proposed Changes)

### 1. 커널 (Kernel)
- **`kernel.c`**: 
    - `PrintString` 함수에서 `\b` 제어 문자를 감지하여 커서를 뒤로 옮기고 배경색으로 글자를 지우는 로직 추가.
    - `ScrollUp` 함수를 추가하여 화면의 모든 픽셀을 한 행(16 픽셀)만큼 위로 복사하고 마지막 행을 초기화.
    - 커서가 화면 하단을 벗어날 때 `ScrollUp` 호출 및 `cursor_y` 조정.

### 2. 시스템 콜 (System Calls)
- **`syscalls.c`**:
    - `read` 시스템 콜에서 `Keyboard_GetChar()`로부터 `\b`를 받았을 때 입력 버퍼 인덱스를 감소시켜 실제 버퍼에서 문자가 제거되도록 수정.

## 검증 계획 (Verification Plan)
- `make all` 빌드 확인.
*   실제 QEMU 환경에서 백스페이스 입력 시 화면 및 내부 버퍼 정상 작동 확인.
*   화면 끝까지 문자를 출력했을 때 스크롤이 자연스럽게 일어나는지 확인.
