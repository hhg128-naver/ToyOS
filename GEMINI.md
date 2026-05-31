# ToyOS Project Context (프로젝트 컨텍스트)

이 문서는 ToyOS 프로젝트의 구조, 개발 환경, 빌드 및 실행 방식에 대한 최신 정보를 제공합니다.

## 프로젝트 개요 (Project Overview)
- **목적**: x86_64 아키텍처 기반의 네이티브 UEFI 운영체제 커널 개발.
- **주요 기술 (Main Technologies)**:
    - **Assembly**: NASM (64비트 컨텍스트 스위칭, 인터럽트 핸들러, 시스템 콜 엔트리)
    - **C Language**: GCC (커널 메인 로직, 메모리 관리, 멀티태스킹, Newlib 통합)
    - **Linker**: GNU LD (64비트 ELF 커널 레이아웃 정의)
    - **Bootloader**: GNU-EFI (UEFI Boot Services 사용, BootInfo 전달)
- **아키텍처 (Architecture)**:
    - **x86_64 Long Mode**: 64비트 모드에서 실행되는 커널.
    - **UEFI 환경**: Legacy BIOS/Multiboot가 아닌 UEFI GOP(Graphics Output Protocol)를 통한 그래픽 출력.
    - **Paging**: 4단계 페이징(PML4, PDPT, PD, PT)을 통한 가상 메모리 관리.

## 빌드 및 실행 (Building and Running)
프로젝트는 `Makefile`을 통해 빌드됩니다.

- **빌드 (Build)**:
  ```bash
  make all
  ```
  부트로더(`bootx64.efi`)와 커널(`kernel`) 바이너리를 생성하고 ISO 이미지를 구성합니다.

- **정리 (Clean)**:
  ```bash
  make clean
  ```

- **실행 (Run)**:
  ```bash
  make run-uefi-hdd
  ```
  QEMU 에뮬레이터에서 OVMF 펌웨어를 사용하여 UEFI 모드로 실행합니다.

## 주요 파일 및 구조 (Key Files & Structure)
- `src/bootloader/uefi/`: GNU-EFI 기반 UEFI 부트로더 소스.
- `src/kernel/`: 64비트 커널 소스 (GDT, IDT, PMM, VMM, Task, Syscall 등).
- `src/kernel/asm_utils.asm`: 저수준 프로세서 제어를 위한 어셈블리 유틸리티.
- `doc/`: 상세 구현 계획 및 단계별 완료 보고서.

## 개발 컨벤션 및 주의사항 (Development Conventions)
- **64비트 타겟**: `-m64`, `-ffreestanding`, `-mcmodel=kernel` 등의 플래그를 사용하여 컴파일됩니다.
- **메모리 접근**: UEFI에서 전달받은 `BootInfo`의 프레임버퍼 주소를 통해 화면을 제어합니다.
- **표준 라이브러리**: `Newlib`이 통합되어 있어 `printf`, `malloc` 등을 커널 내에서 사용할 수 있습니다.

## 현재 진행 상태 (Current Status)
- [x] UEFI 부트로더 및 64비트 커널 핸드오버.
- [x] 그래픽 콘솔 (GOP + PSF Font).
- [x] 메모리 관리 (PMM, VMM, Kernel Heap).
- [x] 인터럽트(PIC, Keyboard, Timer) 및 예외 처리.
- [x] 멀티태스킹 (Preemptive Scheduling).
- [x] 유저 모드(Ring 3) 전환 및 시스템 콜(System Call).
- [x] FPU/SSE 활성화 및 Newlib 연동.

## AI Agent에게 보내는 작업 지침
- 기존의 주석의 내용이 틀린 것이 아니라면 되도록 유지.
- 새로 추가한 파일은 커밋 요청이 있을 때, git add 를 진행하도록 함.
- 답변을 마크다운(Markdown) 코드 블록 형태로 화면에 출력해 줘