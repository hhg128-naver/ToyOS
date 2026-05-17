# ToyOS 프로젝트 현재 진행 상태 보고서 (2026-05-16)

이 문서는 ToyOS 프로젝트의 현재까지의 개발 진행 상황과 향후 계획을 정리한 보고서입니다.

## 1. 구현 완료된 주요 기능 (Completed Features)

### 🚀 부팅 및 커널 기초 (Booting & Kernel Base)
- **UEFI Bootloader**: GNU-EFI를 사용하여 GOP(Graphics Output Protocol) 정보를 획득하고 64비트 커널로 제어권을 넘깁니다.
- **GDT/IDT**: 64비트 세그먼테이션과 인터럽트 기술자 테이블을 설정하여 시스템의 기초를 구축했습니다.
- **Interrupt Handling**: PIC를 통한 타이머(Timer) 및 키보드(Keyboard) 인터럽트 처리가 가능하며, 32개의 CPU 예외(Exception) 핸들러가 구현되어 있습니다.

### 🧠 메모리 관리 (Memory Management)
- **PMM (Physical Memory Manager)**: 비트맵 방식을 사용하여 물리 페이지 프레임을 관리합니다.
- **VMM (Virtual Memory Manager)**: 4단계 페이징(PML4, PDPT, PD, PT)을 구현하여 가상 주소 공간을 관리합니다.
- **Kernel Heap**: `kmalloc` 및 `kfree`를 통해 동적 메모리 할당이 가능합니다.

### 🎭 멀티태스킹 및 사용자 모드 (Multitasking & User Mode)
- **Preemptive Scheduler**: 타이머 인터럽트를 기반으로 한 라운드 로빈(Round-Robin) 스케줄러가 동작합니다.
- **User Mode (Ring 3)**: 커널 권한에서 사용자 권한으로의 안전한 전환을 지원합니다.
- **System Call (syscall)**: `syscall` 명령어를 사용하여 유저 모드에서 커널 서비스를 요청할 수 있습니다.
- **Address Space Isolation**: 각 사용자 태스크별로 독립된 페이지 테이블(PML4)을 할당하여 주소 공간을 격리했습니다.

### 📚 표준 라이브러리 및 쉘 (Library & Shell)
- **Newlib Integration**: 표준 C 라이브러리인 Newlib을 이식하여 `printf`, `malloc`, `string` 관련 함수들을 커널 및 유저 모드에서 사용할 수 있습니다.
- **Interactive Shell**: 기본적인 명령어를 처리할 수 있는 대화형 쉘이 구현되어 있습니다.

### 💾 저장 장치 및 파일 시스템 (Storage & FileSystem)
- **IDE Driver**: 하드 디스크(PIO 모드)에서 섹터 단위로 데이터를 읽고 쓸 수 있는 드라이버를 구현했습니다.
- **VFS (Virtual File System)**: 파일 시스템의 추상화 레이어를 구축하여 다양한 파일 시스템을 통합할 수 있는 기반을 마련했습니다.
- **FAT32 (Read-only)**: FAT32 파티션을 인식하고, 루트 디렉터리의 파일 목록(`ls`)과 파일 내용(`cat`)을 읽는 기능을 구현했습니다.

## 2. 최근 작업 요약 (Recent Updates)

- **VFS/FAT32 구현 완료**: 디스크 파티션에서 부트 섹터(BPB)를 파싱하고, 클러스터 체인을 따라 파일 데이터를 읽어오는 로직을 완성했습니다.
- **Shell 명령어 확장**: 파일 시스템과 연동되는 `ls`와 `cat` 명령어를 추가하여 실제 디스크의 데이터를 확인할 수 있게 되었습니다.

## 3. 현재 위치 및 향후 과제 (Next Steps)

현재 ToyOS는 **"디스크에서 프로그램을 읽어 실행할 수 있는 운영체제"**로 넘어가는 과도기에 있습니다.

- **[ ] 경로 파싱 (Path Parsing)**: 현재는 루트 디렉터리의 파일만 접근 가능하지만, `/usr/bin/test`와 같이 경로를 분석하여 하위 디렉터리를 탐색하는 기능이 필요합니다.
- **[ ] ELF 로더 (ELF Loader)**: 현재 유저 태스크는 커널 바이너리에 내장된 함수만 실행 가능합니다. 디스크의 ELF 파일을 읽어 메모리에 로드하고 실행하는 기능이 최우선 과제입니다.
- **[ ] 파일 쓰기 지원 (Write Support)**: FAT32 파일 시스템에 새로운 파일을 생성하거나 내용을 수정하는 기능을 추가해야 합니다.
- **[ ] 긴 파일 이름 (LFN) 지원**: 8.3 포맷 이상의 긴 파일 이름을 정상적으로 표시하기 위한 로직이 필요합니다.

---
**보고서 작성자**: Antigravity (AI Coding Assistant)  
**작성일**: 2026년 5월 16일
