# ToyOS C++ Support Implementation Plan (C++ 지원 구현 계획서)

이 문서는 ToyOS 커널 개발에 C++를 도입하기 위한 단계별 계획을 설명합니다.

## 1. 목적 (Objectives)
- 커널 개발 시 객체 지향 프로그래밍(OOP) 패러다임 활용.
- 클래스, 템플릿 등 C++의 현대적 기능을 사용한 코드 구조 개선.
- 예외(Exceptions) 및 RTTI(Run-Time Type Information)를 제외한 고성능 C++ 환경 구축.

## 2. 상세 단계 (Detailed Steps)

### 단계 1: 툴체인 업데이트 (Toolchain Update)
- `build_toolchain.sh` 수정: `--enable-languages=c`를 `--enable-languages=c,c++`로 변경.
- `x86_64-elf-g++` 컴파일러 빌드 및 설치 확인.

### 단계 2: 링커 스크립트 수정 (Linker Script Modification)
- `src/kernel/link.ld`에 정적 생성자 목록을 위한 섹션 추가.
- `.init_array` 또는 `.ctors` 섹션을 정의하여 정적 객체의 생성자 주소를 수집.

### 단계 3: 정적 생성자 호출 (Global Constructors)
- C++는 전역/정적 객체의 생성자를 실행 시점에 명시적으로 호출해야 함.
- `src/kernel/kernel.c` 또는 별도 파일에 `__libc_init_array` 역할을 수행할 루틴 구현.

### 단계 4: 필수 C++ 런타임 지원 (C++ Runtime Support)
- **Memory Management**: `new` 및 `delete` 연산자를 커널의 `kmalloc`/`kfree`와 매핑.
- **Error Handling**: `__cxa_pure_virtual` (순수 가상 함수 호출 시 에러 처리) 등 필수 심볼 정의.
- **Compile Flags**: `-fno-exceptions`, `-fno-rtti`, `-fno-use-cxa-atexit` 등을 사용하여 커널에 부적합한 기능 비활성화.

### 단계 5: 빌드 시스템 업데이트 (Makefile Update)
- `CROSS_GXX` 변수 정의.
- `.cpp` 확장자에 대한 컴파일 규칙 추가.
- `LDFLAGS`에 C++ 표준 라이브러리(`libstdc++`) 대신 필요한 경우에만 최소한의 링크 설정.

## 3. 검증 계획 (Verification Plan)
- 간단한 C++ 클래스를 정의하고 전역 객체로 선언.
- 생성자에서 화면에 특정 메시지를 출력하도록 하여 정상 호출 확인.
- 가상 함수(Virtual Functions)가 포함된 클래스 상속 구조가 올바르게 작동하는지 테스트.

## 4. 주의사항 (Precautions)
- **No Exceptions**: 커널 레벨에서 예외 처리는 오버헤드가 크고 복잡하므로 비활성화함.
- **No RTTI**: `dynamic_cast` 및 `typeid` 사용 불가.
- **Standard Library**: `std::` 네임스페이스의 대다수 기능은 OS 지원이 필요하므로 직접 구현하거나 최소화하여 사용.
