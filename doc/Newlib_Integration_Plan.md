# Newlib 통합 및 크로스 컴파일러 구축 계획서

## 1. Objective (목표)
ToyOS 커널 개발 환경을 표준화하기 위해 **x86_64-elf 크로스 컴파일러(Binutils, GCC)** 를 구축하고, 이를 이용해 커널용 표준 C 라이브러리인 **newlib**을 소스 코드부터 빌드하여 커널에 통합합니다.

## 2. Key Context & Environment (주요 컨텍스트)
- **대상 타겟**: `x86_64-elf` (운영체제 없는 순수 x86_64 64비트 바이너리)
- **설치 경로**: 현재 사용자 디렉토리(예: `~/.local/opt/cross` 또는 프로젝트 내부 `toolchain/`)에 국한하여 시스템 환경을 오염시키지 않음.
- **포함 요소**: `binutils`, `gcc` (C 컴파일러만), `newlib`

## 3. Implementation Steps (구현 단계)

### Phase 1: 크로스 컴파일러 빌드 환경 준비
- 빌드에 필요한 패키지(빌드 툴, gmp, mpfr, mpc 등)가 호스트에 설치되어 있는지 확인합니다.
- 프로젝트 최상단에 `toolchain/src` 디렉토리를 생성하고 `binutils`, `gcc`, `newlib`의 소스 코드 압축 파일을 다운로드 후 압축을 해제합니다.
- 설치될 컴파일러 경로(예: `$HOME/opt/cross`)를 환경 변수 `PREFIX`와 `TARGET`으로 설정합니다.

### Phase 2: x86_64-elf Binutils 및 GCC (1차) 빌드
- **Binutils**: `target=x86_64-elf`로 설정하여 어셈블러, 링커 등 기본 도구들을 빌드하고 설치합니다.
- **GCC (Core)**: `target=x86_64-elf`로 설정하고 `--without-headers` 옵션을 주어 `newlib` 없이 C 컴파일러 코어만 먼저 빌드합니다. (이 컴파일러를 이용해 newlib을 컴파일해야 합니다.)

### Phase 3: Newlib 빌드 및 최종 GCC 빌드
- **Newlib**: Phase 2에서 빌드한 `x86_64-elf-gcc`를 사용하여 `newlib` 소스코드를 `x86_64-elf` 타겟으로 컴파일하고 설치합니다. 이때 `libc.a`와 `libm.a`가 생성됩니다.
- **GCC (Complete)**: `newlib`이 설치된 후, `--with-newlib` 옵션을 추가하여 GCC를 다시 전체 빌드합니다. 이로써 완벽한 크로스 컴파일러 툴체인이 완성됩니다.

### Phase 4: 커널 소스 및 Makefile 수정 (기존 계획 연동)
- `Makefile`에서 `GCC`와 `LD` 변수를 새로 빌드한 `x86_64-elf-gcc` 및 `x86_64-elf-ld`로 변경합니다.
- `src/kernel/syscalls.c`에 정의된 시스템 콜 스텁(`_sbrk`, `_write`, `_read` 등)이 제대로 컴파일되고, 링커가 `-lc -lgcc`를 성공적으로 찾는지 확인합니다.

## 4. Verification & Testing (검증 및 테스트)
1. **툴체인 검증**: `x86_64-elf-gcc --version` 및 `x86_64-elf-ld -lc -lgcc` 경로 탐색이 정상인지 확인합니다.
2. **커널 빌드 검증**: `make clean && make all` 수행 시 에러 없이 커널 바이너리가 생성되는지 확인합니다.
3. **런타임 검증**: `make run-uefi`로 부팅 시, `kernel.c`에 추가된 `printf`와 `malloc`이 정상 동작하여 화면에 출력되는지 확인합니다.

> **참고**: 툴체인 빌드는 수십 분이 소요될 수 있으므로, 각 단계별 로그를 꼼꼼히 확인하고 자동화 스크립트(`build_toolchain.sh`)를 작성하여 진행할 예정입니다.