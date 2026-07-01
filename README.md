# ToyOS

ToyOS는 x86_64 아키텍처 기반의 네이티브 UEFI 운영체제 커널입니다.

## 🚀 주요 특징 (Key Features)

- **Native UEFI Bootloader**: Legacy BIOS/Multiboot 대신 UEFI 표준 부트 서비스(GOP 등)를 직접 사용하여 시스템을 부팅합니다.
- **64-bit Long Mode Kernel**: 현대적인 64비트 아키텍처 환경에서 커널이 실행됩니다.
- **GOP Graphics Console**: UEFI GOP(Graphics Output Protocol) 및 PSF 폰트를 활용한 고해상도 그래픽 출력을 지원합니다.
- **메모리 및 세그먼트 관리**: PML4 기반 4단계 페이징 가상 메모리 관리자(VMM), 물리 메모리 관리자(PMM), GDT/IDT 및 커널 힙 시스템을 구축하였습니다.
- **멀티태스킹 및 선점형 스케줄링**: 여러 태스크를 동시에 실행하는 컨텍스트 스위칭 및 타이머 인터럽트 기반 선점형 스케줄러를 지원합니다.
- **유저 모드(Ring 3) 및 시스템 콜**: Ring 3 전환과 Syscall 엔트리를 통한 커널-유저 공간 분리를 처리합니다.
- **Newlib C 표준 라이브러리**: C 표준 라이브러리(printf, malloc 등) 및 FPU/SSE 연산 기능이 커널 내부와 연동되어 있습니다.

## 🛠 개발 환경 (Development Environment)

- **Host OS**: Windows 11 (Windows Subsystem for Linux 2 - Ubuntu 22.04 LTS 이상)
- **Compiler**: Clang / LLVM (ld.lld) & NASM Assembler
- **Target Architecture**: `x86_64-elf`
- **Libraries**: gnu-efi, Newlib (크로스 컴파일 툴체인으로 빌드)
- **Emulator**: QEMU (OVMF UEFI Firmware 사용)

---

## 🏗 빌드 및 실행 방법 (Build & Run)

### 1. 사전 필수 패키지 설치 (WSL 환경)
WSL(Ubuntu) 터미널을 실행하고, 빌드 및 에뮬레이터 실행에 필요한 다음 패키지들을 설치합니다.
```bash
sudo apt update
sudo apt install -y build-essential clang lld nasm gnu-efi ovmf qemu-system-x86 curl git dosfstools
```

### 2. 하드디스크 이미지 및 마운트 폴더 생성
빌드 스크립트가 이미지를 마운트하고 커널을 복사할 수 있도록 빈 디스크 이미지를 포맷하여 준비하고, 마운트 포인트 디렉토리(`mnt`)를 생성합니다.
```bash
# 마운트 포인트 디렉토리 생성 (누락 시 빌드 에러 발생)
mkdir -p mnt

# 64MB 크기의 FAT32 가상 하드디스크 이미지 파일 생성
dd if=/dev/zero of=hdd.img bs=1M count=64
mkfs.vfat -F 32 hdd.img
```

### 3. 전체 프로젝트 빌드
부트로더(`bootx64.efi`)와 커널(`kernel`), 사용자 프로그램을 빌드합니다.
```bash
make all
```

### 4. QEMU 에뮬레이터 실행
가상 하드디스크 이미지에 커널과 부트로더를 심어 UEFI 모드로 실행합니다. (실행 시 `sudo` 마운트 조작을 위한 리눅스 비밀번호 입력이 필요합니다.)
```bash
make run-uefi-hdd
```

