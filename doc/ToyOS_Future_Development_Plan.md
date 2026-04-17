# ToyOS 향후 개발 계획 (ToyOS Future Development Plan)

## 1. 개요 (Overview)
기존 Legacy Multiboot 및 32비트 커널 아키텍처에서 64비트 UEFI 환경으로의 마이그레이션이 1차적으로 성공했습니다 (부트로더가 ELF64 커널을 로드하여 화면에 프레임버퍼 픽셀을 그릴 수 있는 상태). 앞으로는 이 환경을 안정화하고 полноценный(full-featured) 커널 시스템의 기틀을 마련해야 합니다.

## 2. 향후 단계별 개발 로드맵 (Phased Roadmap)

### 1단계: BootInfo 확장 및 UEFI 메모리 맵 핸드오버
- **배경**: 현재 부트로더에서 `GetMemoryMap`을 호출하여 메모리 정보를 수집하지만, 이를 커널(`kmain`) 인자로 넘겨주지 않고 있습니다. 커널은 물리 메모리 구성을 알아야 스스로 메모리 관리자를 초기화할 수 있습니다.
- **수행 항목**:
  - `efi_main.c` 내부의 `BootInfo` 구조체에 Memory Map 포인터, Descriptor 크기, Map 전체 크기 항목을 추가.
  - `GetMemoryMap` 획득 후 해당 정보를 `BootInfo`를 통해 `kernel.c`로 전달.
- **기대 효과**: 커널이 전체 가용 메모리 크기와 시스템에 예약된 영역을 인지하게 됨.

### 2단계: 프레임버퍼 기반 텍스트 렌더링 (그래픽 콘솔) [IN PROGRESS]
- **배경**: UEFI 환경의 GOP(Graphics Output Protocol)는 레거시 VGA 텍스트 모드(`0xB8000` 직접 접근)를 지원하지 않으므로, 픽셀 단위로 직접 글자를 그려야(Rendering) 합니다.
- **수행 항목**:
  - [ ] 8x16 비트맵 폰트 데이터 정의 (`font.h` 추가).
  - [ ] 좌표(x, y)에 특정 색상의 문자를 그리는 `PutChar` 구현.
  - [ ] 전역 커서(X, Y) 및 개행(`\n`) 처리 로직이 포함된 `PrintString`.
  - [ ] 화면 하단 도달 시 스크롤 처리 또는 화면 초기화 로직 검증.
- **기대 효과**: 디버깅 메시지를 원활하게 모니터에 출력할 수 있음.

### 3단계: 프로세서 기초 환경 설정 (GDT/IDT)
- **배경**: UEFI 펌웨어가 설정해 둔 GDT/IDT는 커널이 사용하기 부적합할 수 있으므로, 커널의 생명주기를 완벽히 제어하기 위해 독자적인 Descriptor Table을 만들어야 합니다.
- **수행 항목**:
  - 64비트 Long Mode 환경에 맞는 GDT(Global Descriptor Table) 정의 및 로드(`lgdt`).
  - 예외(Exception)와 인터럽트(Interrupt) 처리를 위한 IDT(Interrupt Descriptor Table) 구축.
  - Page Fault, General Protection Fault 등을 잡기 위한 기본 Exception Handler 작성(화면에 블루스크린/에러 출력).
- **기대 효과**: 커널의 안정성 확보 및 예기치 않은 오류 발생 시 원인 파악 용이.

### 4단계: 메모리 관리 시스템 (Memory Management) [COMPLETED]
- **배경**: 1단계에서 전달받은 UEFI 메모리 맵을 토대로 OS 자체의 메모리 관리를 시작합니다.
- **수행 항목**:
  - [x] 물리 메모리 할당자(Physical Memory Manager, PMM): Bitmap 등의 구조를 활용하여 4KB Page 단위의 메모리 할당/해제 구현.
  - [x] 가상 메모리 관리(Paging/VMM): 4단계 페이징 구조 정비 및 Identity 매핑 수행.
  - [x] 커널 힙(Kernel Heap): `kmalloc`, `kfree` 기능을 제공하는 바이트 단위 할당자 구현.
- **기대 효과**: 동적 메모리 사용 가능 및 프로세스 분리 기반 마련.

### 5단계: 인터럽트 컨트롤러(PIC/APIC) 및 키보드 입력
- **배경**: 현재는 키 입력을 받을 수 없어 상호작용이 불가능합니다.
- **수행 항목**:
  - Legacy PIC(8259A) 또는 최신 APIC 활성화 및 설정.
  - 시스템 타이머(PIT 또는 Local APIC Timer) 초기화 (스케줄링 기반).
  - PS/2 키보드 인터럽트(IRQ 1) 핸들러 구현 및 간단한 셸(Shell) 입력 환경 마련.
- **기대 효과**: 사용자 입력에 반응하는 진짜 대화형 운영체제 상태로 진입.

## 3. 검증 및 다음 단계 (Verification)
각 단계를 구현할 때마다 `make run-uefi` (QEMU + OVMF) 환경에서 오류 없이 진행되는지 확인해야 하며, 5단계까지 완료되면 기초적인 응용 프로그램이나 파일 시스템 기능 확장(FAT32 파싱 등)으로 나아갈 수 있습니다.
