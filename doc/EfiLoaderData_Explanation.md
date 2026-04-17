# EfiLoaderData 및 UEFI 메모리 유형 설명

이 문서는 `src/bootloader/uefi/efi_main.c`에서 사용되는 `EfiLoaderData` 상수의 의미와 UEFI 메모리 관리 체계에 대해 설명합니다.

## 1. EfiLoaderData 개요
`EfiLoaderData`는 UEFI 사양에 정의된 `EFI_MEMORY_TYPE` 열거형(Enumeration)의 한 종류입니다. GNU-EFI 라이브러리의 `<efi.h>` 헤더 파일에 정의되어 있습니다.

## 2. 주요 역할
UEFI 환경에서 메모리를 할당할 때(`AllocatePool`, `AllocatePages`), 시스템은 해당 메모리가 어떤 용도로 사용되는지 알아야 합니다. 그래야 부팅 이후(OS 커널 실행 시점)에 해당 메모리를 보존할지, 아니면 즉시 재사용할지 결정할 수 있기 때문입니다.

`EfiLoaderData`는 다음과 같은 의미를 갖습니다:
- **Loader 전용 데이터**: UEFI 로더(부트로더)가 커널을 로드하거나 부팅 관련 정보를 저장하기 위해 사용하는 데이터 영역입니다.
- **휘발성**: OS 커널이 완전히 제어권을 잡고 자체적인 메모리 관리를 시작하면, 이 영역은 운영체제에 의해 일반 메모리로 회수되어 재사용될 수 있습니다. (Runtime Services 영역과는 다름)

## 3. 코드 내 활용 (efi_main.c)
ToyOS 부트로더에서는 다음과 같은 용도로 `EfiLoaderData`를 사용합니다:
1. **ELF 헤더 및 프로그램 헤더 저장**: ELF 파일을 분석하기 위해 메모리에 임시로 올릴 때 사용합니다.
2. **커널 세그먼트 로딩**: `kernel` 바이너리의 내용을 실제 메모리 주소에 배치할 때 해당 영역을 `EfiLoaderData`로 할당합니다.
3. **BootInfo 구조체**: 커널에 넘겨줄 프레임버퍼 정보 등을 담은 구조체를 할당할 때 사용합니다.

## 4. 참고: 주요 메모리 유형 (EFI_MEMORY_TYPE)
- `EfiLoaderCode`: 로더의 실행 코드가 담긴 영역.
- `EfiLoaderData`: 로더가 사용하는 데이터 영역.
- `EfiRuntimeServicesCode/Data`: OS 실행 중에도 유지되어야 하는 UEFI 런타임 서비스 관련 영역.
- `EfiConventionalMemory`: 아직 할당되지 않은 일반 가용 메모리.

---
*작성일: 2026-04-15*
*관련 파일: src/bootloader/uefi/efi_main.c*
