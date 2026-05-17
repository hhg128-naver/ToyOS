# ToyOS C++ 지원 현황 및 도입 가이드

이 문서는 ToyOS 커널에 C++를 도입하기 위한 현재 상태 분석과 필요한 기술적 요구사항을 정리합니다.

## 1. 현재 상태 분석 (Current Status)

- **Compiler**: 현재 `x86_64-elf-gcc`를 사용 중입니다. 이 컴파일러는 C++ 코드를 컴파일할 수 있는 기능을 내장하고 있습니다. (확인 결과 `src/kernel/cpp_test.cpp` 컴파일 성공)
- **Standard Library**: 커널 환경이므로 `libstdc++`와 같은 표준 라이브러리는 사용할 수 없습니다. `Newlib`의 C 라이브러리 기능만 활용 가능합니다.
- **Missing Parts**: 전역 생성자 호출 로직, `new/delete` 연산자 정의, C++ 전용 컴파일 옵션이 `Makefile`에 아직 적용되지 않았습니다.

## 2. C++ 도입을 위한 필수 구현 사항

### A. 컴파일 및 링크 설정 (Makefile)
C++ 파일(`.cpp`)을 위한 빌드 규칙을 추가해야 하며, 다음 플래그가 필수적입니다.
- `-fno-exceptions`: 커널에서 오버헤드가 큰 예외 처리 기능을 제거합니다.
- `-fno-rtti`: 런타임 타입 정보를 제거하여 바이너리 크기를 줄입니다.
- `-ffreestanding`: 표준 라이브러리가 없는 환경임을 명시합니다.

### B. 필수 런타임 함수 (Runtime Support)
C++ 언어 기능을 지원하기 위해 커널 내에 다음 함수들을 정의해야 합니다.

```cpp
// src/kernel/cpp_runtime.cpp (예시)
extern "C" void __cxa_pure_virtual() {
    // 순수 가상 함수 호출 시 패닉 처리
}

void* operator new(unsigned long size) {
    return kmalloc(size);
}

void operator delete(void* p, unsigned long size) {
    kfree(p);
}
// delete[] 등 추가 필요
```

### C. 전역 생성자 초기화
전역/정적 객체의 생성자가 실행되도록 `link.ld`에서 `.init_array` 섹션을 정의하고, 커널 시작 시 이를 순회해야 합니다.

```c
// kernel.c (예시)
typedef void (*constructor)();
extern constructor start_ctors;
extern constructor end_ctors;

void call_constructors() {
    for (constructor* i = &start_ctors; i < &end_ctors; i++) {
        (*i)();
    }
}
```

## 3. 추천 도입 경로

1. **Clang 전환과 통합**: 현재 `doc/Plan_Switch_To_Clang.md` 계획에 따라 Clang으로 전환하면서 `clang++` 환경을 구축하는 것이 가장 현대적인 방식입니다.
2. **GCC에서 선제 도입**: Clang 전환 전에 C++의 클래스 구조 등을 먼저 활용하고 싶다면, 현재 `Makefile`에 `.cpp` 지원을 먼저 추가할 수 있습니다.

## 4. 향후 계획 (TODO)
- [ ] `Makefile`에 C++ 컴파일 규칙 추가.
- [ ] `src/kernel/cpp_runtime.cpp` 생성 및 필수 연산자 구현.
- [ ] `link.ld` 수정 및 전역 생성자 호출 로직 추가.
- [ ] 기존 C 커널 헤더들에 `extern "C"` 가드 적용.
