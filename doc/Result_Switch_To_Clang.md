# Clang 빌드 시스템 및 C++ 런타임 지원 도입 결과

ToyOS의 빌드 시스템을 GCC에서 Clang/LLVM으로 전환하고, 커널 내 C++ 코드 작성을 위한 기반 런타임을 구현하였습니다.

## 주요 변경 사항

### 1. `Makefile` 컴파일러 및 링커 변경
- 기존 `x86_64-elf-gcc` 대신 `clang` 및 `clang++`를 사용하도록 변경했습니다.
- 링크 과정에서 `ld.lld`를 직접 호출하여 처리 속도와 모던 링커 기능을 확보했습니다.
- C++(`*.cpp`) 컴파일을 위한 새로운 빌드 규칙을 추가하였습니다 (`-fno-exceptions`, `-fno-rtti` 적용).

### 2. C++ 런타임 환경 구성
- **`.init_array` 섹션 추가**: C++의 전역 객체 생성자가 위치하는 `.init_array`를 `link.ld` 링커 스크립트에 정의했습니다.
- **`cpp_support.cpp` 구현**: 
  - `call_constructors()`를 통해 커널 시작 시 `.init_array`에 등록된 전역 객체들의 생성자를 순차적으로 호출하도록 했습니다.
  - 가상 함수 오류 처리를 위한 `__cxa_pure_virtual` 스텁을 작성했습니다.
  - `operator new`와 `operator delete`를 `kmalloc` / `kfree` 커널 힙 할당자와 연결했습니다.

### 3. 커널 메인 로직 연동
- `kernel.c`에서 힙 초기화 직후 `call_constructors()`를 호출하도록 설정하여, C++ 클래스의 글로벌 인스턴스들이 문제없이 초기화될 수 있도록 구성했습니다.

## 빌드 검증
```bash
wsl make clean && wsl make all
```
명령을 통해 커널과 부트로더가 Clang 기반으로 성공적으로 빌드됨을 자동화된 환경에서 검증했습니다.

## 수동 확인 요청
> [!IMPORTANT]
> 에뮬레이터(QEMU) 그래픽 출력은 현재 백그라운드 환경에서 직접 확인할 수 없습니다. 
> 시스템의 안정적인 부팅 확인을 위해 터미널에서 다음 명령을 실행하여 정상 동작 여부를 검증해주시기 바랍니다.
> ```bash
> make run-uefi
> ```
