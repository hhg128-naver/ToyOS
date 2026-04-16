# EFI_SYSTEM_TABLE 구조 가이드

이 문서는 UEFI 환경의 핵심 구조체인 `EFI_SYSTEM_TABLE`의 구조와 역할에 대해 설명합니다.

## 1. 개요
`EFI_SYSTEM_TABLE`은 UEFI 펌웨어가 제공하는 모든 서비스와 시스템 정보에 접근할 수 있는 루트 구조체입니다. UEFI 애플리케이션의 엔트리 포인트인 `efi_main` 함수의 두 번째 인자로 전달됩니다.

## 2. 주요 멤버 구성

| 멤버 이름 | 타입 | 설명 |
| :--- | :--- | :--- |
| `Hdr` | `EFI_TABLE_HEADER` | 테이블 시그니처, 버전, 체크섬 정보 |
| `FirmwareVendor` | `CHAR16 *` | 펌웨어 제조사 이름 (유니코드 문자열) |
| `ConIn` | `EFI_SIMPLE_TEXT_INPUT_PROTOCOL *` | 표준 입력 장치 (키보드 등) 제어 |
| `ConOut` | `EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *` | 표준 출력 장치 (화면) 제어 |
| `StdErr` | `EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *` | 표준 에러 출력 장치 |
| `RuntimeServices` | `EFI_RUNTIME_SERVICES *` | OS 부팅 후에도 사용 가능한 서비스 (시간, NVRAM 등) |
| `BootServices` | `EFI_BOOT_SERVICES *` | 부팅 중에만 사용 가능한 서비스 (메모리, 프로토콜, 이벤트 등) |
| `ConfigurationTable`| `EFI_CONFIGURATION_TABLE *` | ACPI, SMBIOS 등 기타 시스템 테이블 목록 |

## 3. 핵심 서비스 상세

### 3.1 BootServices (BS)
가장 빈번하게 사용되는 서비스로, 다음과 같은 기능을 포함합니다:
- **Task Priority**: 인터럽트 우선순위 관리
- **Memory Services**: `AllocatePages`, `AllocatePool`, `FreePages`, `GetMemoryMap` 등
- **Protocol Services**: `LocateProtocol`, `HandleProtocol` 등 (하드웨어 인터페이스 접근)
- **Image Services**: 외부 바이너리 로드 및 실행

### 3.2 RuntimeServices (RT)
커널이 제어권을 넘겨받은 후에도 유지되는 서비스입니다:
- **Variable Services**: UEFI 환경 변수(NVRAM) 읽기/쓰기
- **Time Services**: 실시간 시계(RTC) 접근
- **Miscellaneous Services**: 시스템 리셋(ResetSystem) 등

## 4. 코드 활용 (ToyOS 예시)
`efi_main.c`에서는 주로 `SystemTable->BootServices`를 통해 하드웨어 자원을 확보하고 커널을 메모리에 배치하는 작업을 수행합니다. `ST`라는 약어로 사용되기도 합니다.

---
*작성일: 2026-04-15*
*관련 파일: src/bootloader/uefi/efi_main.c*
