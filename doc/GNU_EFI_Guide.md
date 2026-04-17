# 🛠 gnu-efi 상세 기술 가이드 (Detailed Guide to gnu-efi)

이 문서는 ToyOS 프로젝트에서 사용 중인 **gnu-efi** 라이브러리의 역할, 내부 구조 및 주요 기능에 대해 상세히 설명합니다.

---

## 1. gnu-efi란? (What is gnu-efi?)

**gnu-efi**는 GCC(GNU Compiler Collection)를 사용하여 UEFI(Unified Extensible Firmware Interface) 애플리케이션이나 드라이버를 개발할 수 있도록 돕는 오픈 소스 라이브러리 및 툴체인 세트입니다.

### 핵심 역할 (Key Roles)
1. **형식 변환 (Format Conversion)**: GCC가 생성한 ELF 바이너리를 UEFI가 인식할 수 있는 PE-COFF(`.efi`) 형식으로 변환할 수 있는 환경(링커 스크립트 및 빌드 규칙)을 제공합니다.
2. **호출 규약 브릿지 (Calling Convention Bridge)**: 
    - **GCC (System V ABI)**: x86_64 리눅스 표준 호출 규약.
    - **UEFI (Microsoft x64 ABI)**: Windows/UEFI 표준 호출 규약.
    - `gnu-efi`는 이 두 규약 사이를 연결해주는 래퍼(`uefi_call_wrapper`)를 제공합니다.

---

## 2. 주요 구성 요소 (Core Components)

### A. 헤더 파일 (Header Files)
- `efi.h`, `efilib.h`: UEFI 사양에 정의된 데이터 구조체, 프로토콜 GUID, 서비스 테이블(`SystemTable`, `BootServices` 등)을 C 언어로 정의합니다.
- 모든 문자열은 16비트 유니코드(`UTF-16`)를 사용하므로 문자열 앞에 `L`을 붙여야 합니다 (예: `L"Hello"`).

### B. `crt0` (C Runtime Start)
- UEFI 펌웨어가 `.efi` 파일을 로드할 때 가장 먼저 실행되는 로우 레벨 진입점 코드입니다. (`crt0-efi-x86_64.o`)
- 스택을 정렬하고 레지스터를 설정한 뒤, 사용자가 정의한 `efi_main`을 호출합니다.

### C. 링커 스크립트 (Linker Scripts)
- `elf_x86_64_efi.lds`: ELF 섹션을 PE-COFF 규격에 맞게 배치합니다.
- `.text`, `.data`, `.reloc`(재배치 섹션) 등의 배치를 결정하며, UEFI 이미지가 메모리에 올바르게 로드되도록 보장합니다.

---

## 3. 핵심 함수 및 매크로 (Key Functions)

### `uefi_call_wrapper()`
UEFI 서비스(함수)를 호출할 때 호출 규약을 변환해주는 핵심 매크로입니다.
```c
// 예: LocateProtocol 호출
Status = uefi_call_wrapper(SystemTable->BootServices->LocateProtocol, 3, &ProtocolGuid, NULL, &Interface);
```
- 첫 번째 인자: 호출할 함수 주소.
- 두 번째 인자: 함수가 받는 인자의 개수.
- 나머지: 함수에 전달할 실제 인자들.

### `InitializeLib()`
- UEFI 애플리케이션 시작 시 라이브러리를 초기화합니다.
- `Print()` 함수와 같은 편리한 입출력 기능을 사용할 수 있게 전역 변수를 설정합니다.

---

## 4. gnu-efi vs EDK II 비교

| 비교 항목 | gnu-efi | EDK II (TianoCore) |
| :--- | :--- | :--- |
| **성격** | 가볍고 단순한 라이브러리 세트 | 공식적이고 방대한 프레임워크 |
| **복잡도** | 낮음 (기존 Makefile 활용 가능) | 높음 (전용 빌드 시스템 `build` 사용) |
| **용도** | **부트로더**, 간단한 유틸리티 | **실제 펌웨어 개발**, 복잡한 드라이버 |
| **도구** | 표준 GCC, Binutils | 전용 툴체인 설정 필요 |

---

## 5. 개발 팁 및 주의사항 (Development Tips)

1. **상태 코드 확인**: 거의 모든 UEFI 함수는 `EFI_STATUS`를 반환합니다. `EFI_ERROR(Status)` 매크로를 사용하여 오류 여부를 항상 체크하십시오.
2. **페이지 단위 할당**: 커널 로딩 시에는 `AllocatePool`보다 `AllocatePages`를 사용하여 4KB 페이지 단위로 메모리를 관리하는 것이 아키텍처 설계에 유리합니다.
3. **가변 인자 함수**: `Print()`와 같은 가변 인자 함수는 `uefi_call_wrapper` 없이 직접 호출할 수 있도록 라이브러리 차원에서 구현되어 있는 경우가 많습니다.

---
*마지막 업데이트: 2026-04-14*
