# GCC에서 Clang으로 빌드 시스템 전환 계획

이 문서는 ToyOS의 빌드 시스템을 기존 GCC 기반에서 Clang/LLVM 기반으로 전환하기 위한 상세 계획을 담고 있습니다.

## 목적
- Clang의 빠른 컴파일 속도와 우수한 에러 메시지 활용.
- LLVM 도구 체인(LLD, llvm-objcopy 등)을 통한 빌드 프로세스 현대화.
- **C++ 지원 환경 마련**: `clang++`를 사용하여 Newlib 기반의 C++ 커널 코드 작성이 가능하도록 설정.
- 향후 정적 분석 및 최적화 기능 활용 기반 마련.

## 사용자 검토 필요 사항
> [!IMPORTANT]
> 빌드 환경(WSL 등)에 `clang`과 `lld`가 설치되어 있어야 합니다. 설치되어 있지 않다면 다음 명령어를 통해 설치가 필요합니다:
> `sudo apt update && sudo apt install clang lld`

> [!WARNING]
> 현재 `newlib`과 `libgcc`는 GCC로 빌드되어 있습니다. Clang으로 링크할 때 호환성 이슈가 발생할 수 있으나, 일반적으로 `x86_64-elf` 타겟에서는 큰 문제가 없습니다. 문제가 발생할 경우 `compiler-rt`로의 교체를 검토해야 합니다.

## 주요 변경 사항

### 1. 환경 설정 (Environment Setup)
- WSL 또는 빌드 환경에 LLVM 도구 모음 설치 확인.
- `Makefile`에서 컴파일러 및 링커 변수 정의 변경.

### 2. [MODIFY] [Makefile](file:///f:/ToyOS/Makefile)
- `CROSS_GCC` -> `CLANG` (명령어: `clang`)
- `CROSS_CXX` -> `CLANGXX` (명령어: `clang++`) [NEW]
- `CROSS_LD` -> `LD_LLD` (명령어: `ld.lld`)
- `CFLAGS` 및 `CXXFLAGS`에 `-target x86_64-elf` 추가.
- `CXXFLAGS`에 `-fno-exceptions`, `-fno-rtti` 추가.
- `LDFLAGS`에서 GNU LD 전용 옵션을 LLD 호환 옵션으로 변경.
- UEFI 빌드 규칙을 `clang`을 사용하도록 업데이트.

### 3. [MODIFY] 커널 및 UEFI 소스 코드 (필요 시)
- **전역 생성자 호출 로직 추가**: `.init_array` 섹션을 순회하며 생성자를 호출하는 함수 구현.
- **C++ 스텁 추가**: `extern "C"` 블록 처리 및 `__cxa_pure_virtual` 등 필수 런타임 함수 정의.
- GCC 전용 속성(`__attribute__`)이나 built-in 함수가 Clang에서 다른 경우 수정.

---

## 단계별 실행 계획

### 1단계: 도구 설치 및 확인
- 빌드 환경에서 `clang --version` 및 `ld.lld --version` 실행 가능 여부 확인.

### 2단계: Makefile 수정 (커널 빌드)
- 커널 컴파일러를 `clang`으로 변경.
- 링커를 `ld.lld`로 변경.
- `-mno-red-zone`, `-ffreestanding` 등 필수 플래그 유지.

### 3단계: Makefile 수정 (UEFI 빌드)
- `HOST_GCC` 대신 `clang` 사용.
- UEFI는 PE/COFF 형식을 사용하므로, Clang의 `-target x86_64-unknown-windows-coff` 옵션을 사용하여 직접 `.efi` 파일을 생성하는 방식도 고려 가능하나, 현재의 `gnu-efi` 흐름을 유지하며 컴파일러만 교체하는 것을 우선으로 함.

### 4단계: 빌드 및 디버깅
- `make clean && make all` 수행.
- 발생하는 경고 및 에러 수정.

---

## 검증 계획

### 자동화 테스트 (빌드 확인)
- `make all`이 에러 없이 완료되는지 확인.
- 생성된 `kernel` 및 `bootx64.efi` 파일의 포맷 확인 (`file` 명령어 사용).

### 수동 검증 (실행 확인)
- `make run-uefi`를 통해 QEMU에서 정상적으로 부팅되는지 확인.
- 화면 출력, 키보드 입력, 멀티태스킹 등 핵심 기능 작동 여부 확인.
