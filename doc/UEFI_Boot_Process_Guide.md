# 🚀 UEFI Native Bootloader & 64-bit Kernel 전환 가이드

이 문서는 ToyOS 프로젝트를 BIOS 기반 아키텍처에서 최신 UEFI(Unified Extensible Firmware Interface) 표준으로 전환하고, 64비트(Long Mode) 커널을 로드하여 그래픽 출력을 수행하는 전 과정을 기록한 상세 가이드입니다.

---

## 1. 개발 환경 구성 (Prerequisites)

UEFI 개발을 위해서는 PE/COFF 바이너리를 생성하고 UEFI 펌웨어 위에서 테스트할 수 있는 도구가 필요합니다.

- **WSL (Ubuntu)**: 리눅스 빌드 환경.
- **gnu-efi**: UEFI 애플리케이션 개발을 위한 C 라이브러리.
- **OVMF**: QEMU에서 UEFI 펌웨어를 에뮬레이션하기 위한 오픈 소스 펌웨어 (`OVMF.fd`).
- **Build Tools**: `gcc`, `ld`, `objcopy`, `nasm`.
- **QEMU**: `qemu-system-x86_64`.

---

## 2. 프로젝트 디렉토리 구조 (Directory Structure)

소스 코드와 산출물을 역할별로 분리하여 관리합니다.

```text
ToyOS/
├── src/
│   ├── bootloader/
│   │   ├── legacy/      # 기존 BIOS/Multiboot 소스 (boot.asm)
│   │   └── uefi/        # 신규 UEFI 부트로더 (efi_main.c)
│   └── kernel/          # 64비트 커널 (kernel.c, kernel.h, link.ld)
├── doc/                 # 설계 문서 및 가이드
├── Makefile             # 빌드 자동화 스크립트
└── .gitignore           # 빌드 산출물 제외 설정
```

---

## 3. 핵심 빌드 전략 (Build Strategy)

UEFI 부트로더와 커널은 서로 다른 형식과 아키텍처를 가지므로 독립적으로 빌드해야 합니다.

### 3.1 UEFI 부트로더 (.efi) 빌드
1. **컴파일**: `-fpic`, `-fshort-wchar`, `-mno-red-zone` 등의 플래그를 사용하여 ELF 오브젝트 생성.
2. **링크**: `gnu-efi` 전용 링커 스크립트(`elf_x86_64_efi.lds`)를 사용하여 공유 라이브러리(`.so`) 생성.
3. **변환**: `objcopy`를 사용하여 ELF를 UEFI 펌웨어가 인식하는 **PE-COFF**(`.efi`) 형식으로 변환.

### 3.2 64비트 커널 (ELF64) 빌드
- **컴파일**: `-m64`, `-ffreestanding`, `-fno-stack-protector`, `-mno-red-zone` 플래그 사용.
- **링크**: 64비트 링커 스크립트(`OUTPUT_FORMAT(elf64-x86-64)`)를 사용하여 특정 주소(예: `0x100000`)에 배치.

---

## 4. 부트로더 핵심 로직 (The Bootloader)

UEFI 부트로더(`efi_main`)는 다음 4가지 핵심 역할을 수행합니다.

### A. GOP (Graphics Output Protocol) 활성화
- BIOS의 VGA 텍스트 모드(`0xb8000`) 대신 **GOP**를 통해 고해상도 그래픽 프레임버퍼 주소를 획득합니다.
- 이 주소를 커널에 전달해야 커널이 픽셀 단위로 화면에 그림을 그릴 수 있습니다.

### B. ELF 커널 로더 (ELF Loader)
- 디스크(FAT32)에서 `kernel` 파일을 읽어 **ELF 헤더**를 파싱합니다.
- 프로그램 헤더(`Phdr`)를 순회하며 `PT_LOAD` 섹션들을 커널이 지정한 물리 주소(`p_paddr`)에 복사합니다.
- **주의**: 부트로더가 64비트라면 커널도 **ELF64** 형식이어야 파싱 구조체가 일치합니다.

### C. 메모리 맵 획득 (Memory Map)
- `GetMemoryMap`을 호출하여 시스템의 가용 메모리 정보를 얻습니다. 이 과정에서 얻은 `MapKey`는 부트 서비스를 종료할 때 필수적입니다.

### D. 핸드오버 (ExitBootServices)
- **가장 중요한 부분**: `ExitBootServices`를 호출하여 UEFI 부트 서비스를 종료하고 CPU 제어권을 커널로 완전히 넘깁니다.
- **오류 방지**: `GetMemoryMap`과 `ExitBootServices` 사이에는 `Print` 등 어떠한 부트 서비스 함수도 호출해서는 안 됩니다. (호출 시 `MapKey`가 무효화되어 `Invalid Parameter` 오류 발생)

---

## 5. 커널과의 통신 (Boot Info)

부트로더는 커널이 실행된 후 하드웨어를 제어할 수 있도록 필수 정보를 구조체에 담아 전달합니다.

```c
typedef struct {
    unsigned int *framebuffer;           // 그래픽 메모리 주소
    unsigned long screen_size;           // 전체 픽셀 수
    unsigned int horizontal_resolution;   // 가로 해상도
    unsigned int vertical_resolution;     // 세로 해상도
} BootInfo;
```

---

## 6. 주요 시행착오와 해결책 (Troubleshooting)

| 문제 상황 | 원인 | 해결 방법 |
| :--- | :--- | :--- |
| `efi.h`를 찾을 수 없음 | `gnu-efi` 패키지 미설치 | `apt-get install gnu-efi` 설치 및 Makefile 경로 지정 |
| 커널 로드 후 검은 화면 | 아키텍처 불일치 (ELF32 커널 vs ELF64 로더) | 커널을 `-m64` 및 `elf64-x86-64` 형식으로 재빌드 |
| `ExitBootServices` Invalid Parameter | `MapKey` 무효화 | `GetMemoryMap` 이후 `Print` 등 모든 부트 서비스 호출 제거 |
| 화면 색상이 이상함 | 픽셀 포맷 불일치 | UEFI GOP는 보통 BGR 또는 RGB 32비트를 사용하므로 픽셀당 4바이트(UINT32) 연산 수행 |

---
*최종 업데이트: 2026-04-14*
