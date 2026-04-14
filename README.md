# ToyOS

ToyOS는 취미용으로 개발 중인 x86_64 아키텍처 기반의 네이티브 UEFI 운영체제 커널입니다.

## 🚀 주요 특징 (Key Features)

- **Native UEFI Bootloader**: BIOS/Multiboot 대신 UEFI 표준 부트 서비스를 직접 사용하여 부팅합니다.
- **64-bit (Long Mode) Kernel**: 현대적인 64비트 아키텍처를 지원하며 64비트 모드에서 커널이 실행됩니다.
- **GOP Graphics Output**: UEFI의 Graphics Output Protocol(GOP)을 활용하여 고해상도 그래픽 프레임버퍼를 제어합니다.
- **ELF Loader**: 부트로더가 디스크의 FAT32 파일 시스템에서 커널(ELF64)을 로드하고 엔트리 포인트로 제어권을 이행합니다.

## 🛠 개발 환경 (Development Environment)

- **OS**: Windows Subsystem for Linux (WSL - Ubuntu)
- **Compiler**: GCC (x86_64-linux-gnu)
- **Library**: gnu-efi
- **Emulator**: QEMU (with OVMF firmware)

## 📁 프로젝트 구조 (Project Structure)

```text
ToyOS/
├── src/
│   ├── bootloader/
│   │   ├── legacy/      # 기존 BIOS용 부트 코드 (참고용)
│   │   └── uefi/        # 네이티브 UEFI 부트로더 소스
│   └── kernel/          # 64비트 커널 메인 소스
├── doc/                 # UEFI 구현 계획 및 기술 가이드
├── Makefile             # 빌드 자동화 스크립트
└── .gitignore           # 빌드 산출물 제외 설정
```

## 🏗 빌드 및 실행 방법 (Build & Run)

### 사전 요구 사항
WSL 환경에서 다음 패키지가 필요합니다:
```bash
sudo apt-get update
sudo apt-get install -y gnu-efi ovmf qemu-system-x86
```

### 전체 빌드
```bash
make all
```

### UEFI 모드 실행 (QEMU)
```bash
make run-uefi
```
성공적으로 실행되면 QEMU 창에 파란 화면과 함께 커널이 그린 그래픽이 나타납니다.

## 📝 관련 문서 (Documentation)

- [UEFI 구현 상세 계획](doc/UEFI_Implementation_Plan.md)
- [UEFI 부팅 및 핸드오버 기술 가이드](doc/UEFI_Boot_Process_Guide.md)

## 🎯 향후 목표 (Roadmap)

- [x] UEFI 네이티브 부트 시스템 및 64비트 커널 핸드오버
- [ ] GDT(Global Descriptor Table) 및 IDT(Interrupt Descriptor Table) 설정
- [ ] 페이징(Paging) 기반 메모리 관리자 구현
- [ ] 기초적인 키보드/마우스 드라이버 개발
- [ ] 멀티태스킹 지원 (Scheduler)
