# ToyOS Project Context (프로젝트 컨텍스트)

이 문서는 ToyOS 프로젝트의 구조, 개발 환경, 빌드 및 실행 방식에 대한 정보를 제공하여 Gemini CLI가 프로젝트를 더 잘 이해하고 도울 수 있도록 합니다.

## 프로젝트 개요 (Project Overview)
- **목적**: 취미용 x86 32비트 운영체제 커널 개발.
- **주요 기술 (Main Technologies)**:
    - **Assembly**: NASM (부트 로더 및 커널 진입점 설정)
    - **C Language**: GCC (커널 메인 로직 구현)
    - **Linker**: GNU LD (메모리 레이아웃 및 엔트리 포인트 정의)
- **아키텍처 (Architecture)**:
    - Multiboot 규격을 준수하는 ELF32 i386 커널.
    - VGA 텍스트 모드 메모리(`0xb8000`)를 직접 제어하여 화면에 출력.

## 빌드 및 실행 (Building and Running)
프로젝트는 `Makefile`을 통해 빌드됩니다.

- **빌드 (Build)**:
  ```bash
  make all
  ```
  `boot.asm`과 `kernel.c`를 컴파일하고 링크하여 `kernel` 바이너리 파일을 생성합니다.

- **정리 (Clean)**:
  ```bash
  make clean
  ```
  오브젝트 파일(`.o`), `kernel` 바이너리 및 `Dependency.dep` 파일을 삭제합니다.

- **실행 (Run)**:
  ```bash
  make run
  ```
  이 명령은 `kernel` 바이너리를 QEMU 에뮬레이터(`qemu-system-i386`)에서 실행합니다.

## 주요 파일 및 구조 (Key Files & Structure)
- `boot.asm`: Multiboot 헤더 정의, 스택 설정 및 `kmain` 호출.
- `kernel.c`: 화면 초기화 및 텍스트 출력 로직(`kmain`, `Printf`).
- `kernel.h`: 커널 함수 선언.
- `link.ld`: 커널 로드 주소(`0x100000`) 및 섹션 배치 정의.
- `Makefile`: 컴파일 및 링크 자동화 규칙.

## 개발 컨벤션 및 주의사항 (Development Conventions)
- **32비트 타겟**: 모든 코드는 `-m32` 플래그를 사용하여 i386 환경으로 컴파일됩니다.
- **메모리 접근**: 하드웨어 가속 없이 VGA 메모리 주소에 직접 접근하여 화면을 제어합니다.
- **UEFI 관련**: `README.md`에는 UEFI 언급이 있으나, 현재 구현은 Multiboot/BIOS 호환 방식입니다. 향후 UEFI 지원을 위한 부트로더 전환이 필요할 수 있습니다.

## 향후 작업 (TODO)
- [x] `Makefile`에 QEMU 실행 타겟 추가.
- [ ] GDT/IDT 설정 및 인터럽트 처리 구현.
- [ ] 키보드 드라이버 등 기초 입출력 시스템 구축.
- [ ] UEFI 지원을 위한 아키텍처 검토.

## 추가 지침
- 계획 및 답변은 모두 md 파일로 만들어서 doc 폴더에 저장.