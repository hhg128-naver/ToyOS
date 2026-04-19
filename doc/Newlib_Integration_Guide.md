# ToyOS Newlib 통합 및 툴체인 구축 가이드

이 문서는 ToyOS 커널에 표준 C 라이브러리(Newlib)를 통합하는 과정에서 발생한 주요 문제점들과 그 해결 방법을 기록합니다. 향후 독립적으로 개발 환경을 재구축할 때 참고하십시오.

---

## 1. 개요 (Overview)
커널 개발에서 호스트 OS(리눅스 등)용 `gcc`를 그대로 사용하면 커널과 라이브러리 간의 바이너리 호환성 및 심볼 충돌 문제가 발생합니다. 이를 해결하기 위해 **x86_64-elf** 타겟의 크로스 컴파일러를 직접 빌드하고, 해당 툴체인으로 `newlib`을 컴파일하여 통합했습니다.

## 2. 주요 문제 및 해결 방법 (Problems & Solutions)

### 문제 1: 크로스 컴파일러(Cross-Compiler)의 필요성
- **증상**: 호스트 `gcc`로 링크 시 리눅스용 표준 라이브러리를 참조하여 커널에서 실행 불가능한 바이너리가 생성됨.
- **원인**: 호스트 `gcc`는 특정 운영체제(Linux)에 종속된 바이너리를 생성하도록 설정되어 있음.
- **해결**: `Binutils`와 `GCC`를 소스에서 빌드하여 `x86_64-elf` 타겟 전용 툴체인을 구축함.

### 문제 2: GCC 빌드 시 필수 라이브러리 누락
- **증상**: GCC `configure` 단계에서 GMP, MPFR, MPC 라이브러리가 없다는 에러 발생.
- **해결**: GCC 소스 디렉토리 내의 `./contrib/download_prerequisites` 스크립트를 실행하여 필요한 소스를 자동으로 다운로드하고 GCC 빌드 시 함께 컴파일되도록 함.

### 문제 3: 시스템 콜 심볼 이름 불일치 (Underscore Issue)
- **증상**: `newlib`의 `libc.a` 링크 시 `sbrk`, `write`, `read` 등이 `undefined reference`로 나타남.
- **원인**: `newlib`은 내부적으로 언더바가 없는 이름(`sbrk`)을 찾지만, 커널에서는 관습적으로 `_sbrk`로 구현했기 때문.
- **해결**: `src/kernel/syscalls.c`에서 함수 이름을 언더바 없이 정의하고, 만약의 경우를 대비해 `__attribute__((alias("...")))`를 사용하여 언더바가 있는 이름과 없는 이름 모두 지원하도록 함.

### 문제 4: 링커 스크립트의 섹션 누락
- **증상**: `printf` 호출 시 사용하는 포맷 문자열(데이터)이 바이너리에 포함되지 않거나 링크 에러 발생.
- **원인**: 기존 `link.ld`가 `.text`, `.data`, `.bss`만 포함하고 있어, `newlib`이 사용하는 `.rodata`나 `.text.*` 섹션이 버려짐.
- **해결**: 링커 스크립트를 수정하여 `*(.text .text.*)`, `*(.rodata .rodata.*)` 등을 명시적으로 포함시킴.

### 문제 5: UEFI 부트로더 빌드 충돌
- **증상**: 크로스 컴파일러로 `efi_main.c` 빌드 시 호스트 시스템의 UEFI 라이브러리와 충돌하여 `_DYNAMIC` 심볼 에러 발생.
- **원인**: `x86_64-elf-gcc`는 베어메탈(Bare-metal) 타겟이므로, 호스트 시스템용으로 빌드된 UEFI 런타임 오브젝트와 호환되지 않음.
- **해결**: `Makefile`에서 컴파일러를 분리함. 커널은 `CROSS_GCC`로, UEFI 부트로더는 호스트의 `gcc`를 사용하도록 설정.

---

## 3. 재구축 단계 (Step-by-Step Rebuild)

### 1단계: 환경 변수 설정
```bash
export TARGET=x86_64-elf
export PREFIX="$(pwd)/toolchain/install"
export PATH="$PREFIX/bin:$PATH"
```

### 2단계: Binutils 빌드
```bash
./configure --target=$TARGET --prefix="$PREFIX" --with-sysroot --disable-nls --disable-werror
make && make install
```

### 3단계: GCC (Core) 빌드
1. 필수 라이브러리 준비: `./contrib/download_prerequisites`
2. 빌드:
```bash
./configure --target=$TARGET --prefix="$PREFIX" --disable-nls --enable-languages=c --without-headers
make all-gcc && make all-target-libgcc
make install-gcc && make install-target-libgcc
```

### 4단계: Newlib 빌드
```bash
./configure --target=$TARGET --prefix="$PREFIX"
make && make install
```

### 5단계: 커널 링크 설정 (Makefile)
- `LDFLAGS`에 `-nostdlib`을 추가하여 표준 시작 파일을 제외.
- `libc.a`와 `libgcc.a`의 **절대 경로**를 오브젝트 파일(`$^`) 뒤에 직접 명시하여 링크 순서 문제를 방지.

---

## 4. 커널 내 Newlib 사용 팁
- **메모리 할당**: `malloc()`은 `syscalls.c`의 `sbrk()`를 호출합니다. 현재 가이드는 `0x02000000` 영역을 고정으로 사용합니다.
- **출력**: `printf()`는 내부적으로 `write(1, ...)`를 호출하며, 이는 다시 커널의 `Printf()` 또는 `PutChar()`로 연결됩니다.
- **주의**: `newlib`은 부동소수점 연산(`libm`) 등 많은 기능을 제공하지만, 커널에서 사용 시 스택 오버플로우나 인터럽트 안전성(Reentrancy)을 항상 고려해야 합니다.
