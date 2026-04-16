# uefi_call_wrapper 사용 가이드 (Guide)

이 문서는 UEFI 개발 시 `gnu-efi` 라이브러리에서 제공하는 `uefi_call_wrapper` 함수의 역할과 사용법을 설명합니다.

## 1. 개요 (Overview)
`uefi_call_wrapper`는 서로 다른 **호출 규약(Calling Convention)** 사이의 브릿지 역할을 하는 함수입니다.

*   **UEFI (MS x64 ABI):** Microsoft에서 정의한 x64 호출 규약을 사용합니다.
*   **GCC (System V ABI):** 리눅스 등 일반적인 GCC 환경에서 사용하는 호출 규약입니다.

두 규약은 인자를 전달하는 레지스터(RCX, RDX, R8, R9 vs RDI, RSI, RDX, RCX...)가 다르기 때문에, 변환 과정 없이 UEFI 함수를 호출하면 시스템이 충돌(Crash)하게 됩니다.

## 2. 함수 프로토타입 (Function Prototype)
```c
// 실제로는 가변 인자를 받는 매크로 또는 내부 함수로 구현되어 있습니다.
uefi_call_wrapper(void *ApiFunction, UINTN ArgCount, ...);
```

*   **ApiFunction**: 호출하려는 UEFI 서비스 함수의 포인터.
*   **ArgCount**: 해당 UEFI 함수에 전달할 인자의 총 개수.
*   **...**: UEFI 함수에 전달할 실제 인자들.

## 3. 사용 예시 (Examples)

### 3.1 출력 서비스 (ConOut)
```c
// gnu-efi 스타일 출력
uefi_call_wrapper(ST->ConOut->OutputString, 2, ST->ConOut, L"Hello, UEFI World!\r\n");
```

### 3.2 부트 서비스 (Boot Services)
```c
// 입력 이벤트 대기
UINTN Index;
uefi_call_wrapper(BS->WaitForEvent, 3, 1, &ST->ConIn->WaitForKey, &Index);

// 메모리 할당
VOID* Buffer;
uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, 4096, &Buffer);
```

## 4. 요약 (Summary)
최신 컴파일러에서는 `ms_abi` 속성을 통해 이 래퍼 없이 직접 호출이 가능하기도 하지만, `gnu-efi` 환경에서 코드의 이식성과 안정성을 보장하기 위해 `uefi_call_wrapper`를 사용하는 것이 권장됩니다.
