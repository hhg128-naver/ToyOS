# UEFI 입력 핸들링 및 WaitForEvent 유의사항

이 문서는 `efi_main.c`에서 사용자 입력을 기다리는 과정에서 발생할 수 있는 문제와 해결 방법을 설명합니다.

## 1. 문제 현상
`BS->WaitForEvent`를 사용하여 키 입력을 기다릴 때, 사용자가 키를 누르지 않았음에도 불구하고 즉시 다음 코드로 넘어가는 현상이 발생할 수 있습니다.

## 2. 원인 분석
1. **잔류 버퍼**: 시스템 초기화 과정이나 이전 단계에서 입력된 키가 UEFI 입력 버퍼에 남아 있는 경우, `WaitForEvent`는 즉시 성공을 반환합니다.
2. **이벤트 미소비**: `WaitForEvent`는 이벤트 상태만 확인할 뿐, 실제 입력 데이터를 버퍼에서 제거하지 않습니다. 따라서 `ReadKeyStroke`를 호출하여 데이터를 읽어내지 않으면 다음 `WaitForEvent` 호출 시에도 동일한 입력이 감지됩니다.

## 3. 해결 방법 (Best Practice)
확실하게 사용자 입력을 기다리려면 다음 단계를 거쳐야 합니다.

### Step 1: 입력 버퍼 초기화
```c
uefi_call_wrapper(ST->ConIn->Reset, 2, ST->ConIn, FALSE);
```

### Step 2: 이벤트 대기 및 키 읽기
단순 대기 후 반드시 `ReadKeyStroke`를 호출하여 입력 이벤트를 완전히 처리해야 합니다.
```c
UINTN Index;
EFI_INPUT_KEY Key;
uefi_call_wrapper(BS->WaitForEvent, 3, 1, &ST->ConIn->WaitForKey, &Index);
uefi_call_wrapper(ST->ConIn->ReadKeyStroke, 2, ST->ConIn, &Key);
```

## 4. 참고 사항
- `uefi_call_wrapper`의 두 번째 인자는 뒤따르는 인자의 개수를 정확히 명시해야 합니다.
- `WaitForEvent`는 가변 인자 함수가 아니지만, GNU-EFI의 `uefi_call_wrapper`를 사용할 때는 규격에 맞춰 인자 개수를 전달해야 합니다.

---
*작성일: 2026-04-15*
*관련 파일: src/bootloader/uefi/efi_main.c*
