# UEFI 네이티브 부트로더 구현 상세 계획 (UEFI Native Bootloader Implementation Plan)

이 문서는 ToyOS 프로젝트의 부팅 방식을 기존 BIOS/Multiboot에서 UEFI(Unified Extensible Firmware Interface) 표준으로 전환하기 위한 **방법 2(네이티브 구현)**의 상세 로드맵을 기술합니다.

---

## 1. 아키텍처 개요 (Architectural Overview)

- **형식 (Format)**: PE/COFF (.efi 실행 파일)
- **진입점 (Entry Point)**: `efi_main`
- **실행 환경 (Environment)**: x86_64 UEFI Firmware
- **주요 프로토콜 (Key Protocols)**: 
    - `Graphics Output Protocol (GOP)`: 화면 출력용
    - `Simple File System Protocol`: 커널 로딩용

---

## 2. 단계별 실행 계획 (Phase-by-Phase Plan)

### 1단계: 개발 환경 구성 (Phase 1: Toolchain Setup)
- [x] **패키지 설치**: `gnu-efi`, `qemu-system-x86`, `ovmf` 설치 확인.
- [x] **디렉토리 구조 변경**: `src/bootloader`, `src/kernel` 등으로 코드 분리 검토.
- [x] **Makefile 업데이트**: UEFI 타겟 빌드 규칙 추가 (PE-COFF 바이너리 생성).

### 2단계: 최소 기능 부트로더 (Phase 2: Minimal Bootloader)
- [x] **efi_main 구현**: UEFI 시스템 테이블(`ST`)을 이용한 기초적인 텍스트 출력.
- [x] **이미지 빌드**: `.efi` 파일을 포함하는 FAT 형식의 가상 디스크 이미지(`EFI/BOOT/BOOTX64.EFI`) 생성 스크립트 작성.
- [x] **QEMU 테스트**: OVMF 펌웨어를 사용하여 정상 부팅 확인.

### 3단계: 그래픽 및 메모리 맵 (Phase 3: Graphics & Memory Map)
- [x] **GOP 활성화**: 프레임버퍼(Framebuffer) 주소, 해상도, 픽셀 형식을 획득.
- [x] **픽셀 렌더링**: 프레임버퍼에 직접 쓰기를 수행하는 기초 `PutPixel`, `ClearScreen` 함수 구현.
- [x] **Memory Map 획득**: `GetMemoryMap()`을 호출하여 시스템 가용 메모리 영역 파악.

### 4단계: ELF 커널 로더 (Phase 4: ELF Kernel Loader)
- [x] **파일 시스템 접근**: `SimpleFileSystem` 프로토콜로 `kernel` 파일 열기.
- [x] **ELF 파싱**: ELF 헤더를 읽어 메모리에 로드할 세그먼트(Segment) 정보 추출.
- [x] **메모리 배치**: 지정된 가상/물리 주소에 커널 코드와 데이터를 배치.

### 5단계: 커널 핸드오버 (Phase 5: Kernel Handover)
- [x] **ExitBootServices**: UEFI 부트 서비스를 종료하고 CPU 제어권을 커널로 이전 준비.
- [x] **부트 정보 전달**: 커널이 필요로 하는 정보(메모리 맵, GOP 정보 등)를 담은 구조체 정의 및 전달.
- [x] **커널 점프**: 64비트 모드로의 완전한 전환 및 커널 진입점 호출.

---

## 3. 기대 효과 (Expected Outcomes)

1. **표준 준수**: 최신 하드웨어와의 호환성 확보.
2. **확장성**: 64비트(Long Mode) 환경으로의 자연스러운 확장.
3. **학습 가치**: 부트로더의 전 과정을 직접 구현함으로써 시스템 하부 구조에 대한 깊은 이해.

---
*마지막 업데이트: 2026-04-14*
