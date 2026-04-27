# 백스페이스 및 화면 스크롤 구현 완료 보고서 (Backspace & Scrolling Completion Report)

쉘 인터페이스의 사용성을 개선하기 위해 백스페이스 입력 처리와 자동 스크롤 기능을 성공적으로 구현하였습니다.

## 구현 내용 (Implementation Details)

### 1. 백스페이스 지원
- **화면 출력**: `kernel.c`의 `PrintString`에서 `\b` 문자를 처리하여 커서를 8픽셀 뒤로 이동시킨 후 공백을 그려 화면에서 문자를 지웁니다.
- **입력 버퍼**: `syscalls.c`의 `read` 시스템 콜에서 `\b` 입력 시 버퍼 인덱스를 감소시켜 실제 입력 데이터에서 해당 문자가 지워지도록 했습니다.

### 2. 자동 스크롤 기능
- **`ScrollUp` 함수**: `kernel.c`에 추가된 이 함수는 프레임버퍼 메모리를 직접 조작하여 모든 픽셀 데이터를 한 행 위로 복사하고 마지막 행을 배경색으로 초기화합니다.
- **트리거**: `PrintString`에서 커서가 하단 해상도 경계를 넘어서면 자동으로 `ScrollUp`이 실행되어 새로운 줄을 확보합니다.

## 테스트 결과 (Test Results)
- **빌드**: `bash -c "make all"`을 통해 경고 없이 컴파일 완료.
- **기능 확인**: QEMU 실행 환경에서 백스페이스를 통한 오타 수정 및 긴 텍스트 출력 시 스크롤 작동을 확인하였습니다.

## 관련 파일
- [kernel.c](file:///f:/ToyOS/src/kernel/kernel.c)
- [syscalls.c](file:///f:/ToyOS/src/kernel/syscalls.c)
- [Backspace_Scrolling_Implementation_Plan.md](file:///f:/ToyOS/doc/Backspace_Scrolling_Implementation_Plan.md)
