# ToyOS 예외 발생 원인 분석 및 해결 보고서

본 문서는 ToyOS 개발 과정에서 발생한 **General Protection Fault (GPF 0x62)** 및 **Invalid Opcode (#UD)** 에러의 기술적 원인과 해결 과정을 상세히 기록합니다.

## 1. 초기 현상 분석: GPF Error Code 0x62

### 에러 코드의 의미
사용자가 보고한 `Error Code 0x62`는 2진수로 `0110 0010`입니다. x86_64 아키텍처의 예외 에러 코드 형식에 따라 분석하면 다음과 같습니다:
- **Bit 0 (EXT)**: 0 (내부 이벤트)
- **Bit 1 (IDT)**: 1 (IDT 엔트리 관련)
- **Bit 2 (TI)**: 0 (GDT 관련 비트이나 IDT 비트가 1이므로 무시)
- **Bits 15:3 (Index)**: `0110 0` = **12 (Decimal)**

즉, CPU가 **IDT의 12번 엔트리(Stack Segment Fault, #SS)**를 참조하려다 실패하여 **General Protection Fault (#GP, 13번)**를 발생시킨 것입니다. 당시 IDT에는 12번 핸들러가 등록되어 있지 않았기 때문에, #SS 발생 시 곧바로 #GP로 전이되었습니다.

## 2. 근본 원인: `insw` 함수의 레지스터 오염

가장 핵심적인 문제는 `src/kernel/asm_utils.asm`에 정의된 `insw` (Input String from Port to Window) 함수의 구현 오류였습니다.

### 기존 코드의 문제점
```nasm
; insw(uint16_t port, void* buffer, uint32_t count)
; x86_64 SysV ABI: RDI=port, RSI=buffer, RDX=count
insw:
    push rdi
    push rcx
    mov dx, di      ; [문제점 1] RDI(port)를 DX에 복사. 
                    ; DX는 RDX의 하위 16비트이므로, RDX에 담긴 count 값이 오염됨!
    mov rdi, rsi    ; buffer 주소를 RDI로 설정
    mov rcx, rdx    ; [문제점 2] 오염된 RDX(count + port 조각)를 RCX에 복사.
    cld
    rep insw        ; 의도한 count보다 훨씬 많은 데이터를 읽게 됨
    ...
```

### 연쇄 반응 (Chain Reaction)
1.  **데이터 오염**: `count`가 256(`0x100`)이고 `port`가 `0x1F0`인 경우, `mov dx, di` 이후 `RDX`는 `0x1F0`되어 버립니다.
2.  **스택 오버플로우 (Stack Overflow)**: `rep insw`가 의도했던 256워드(512바이트)가 아닌 496워드(992바이트)를 읽어옵니다.
3.  **메모리 오염**: `FAT32_Init` 등에서 스택에 할당된 512바이트 버퍼를 넘어 주변의 **Return Address (복귀 주소)** 및 보존된 레지스터들을 덮어쓰게 됩니다.
4.  **#SS 및 #UD 발생**:
    *   함수 종료 시 `ret` 명령어가 오염된 복귀 주소로 점프하려다 비캐노니컬 주소(Non-canonical Address)를 건드리면 **#SS (Stack Segment Fault)**가 발생합니다.
    *   만약 점프한 곳에 유효하지 않은 기계어 코드가 있다면 **#UD (Invalid Opcode)**가 발생합니다.

## 3. 부수적 문제: GDT 매크로 정의 오류

`src/kernel/gdt.h`의 세그먼트 셀렉터(Segment Selector) 정의가 실제 GDT 구성과 일치하지 않았습니다.

- **실적 GDT 구성**: Index 3 = User Data, Index 4 = User Code
- **기존 매크로**: `GDT_USER_CODE`가 Index 3, `GDT_USER_DATA`가 Index 4로 정의됨.

이는 추후 유저 모드(User Mode) 전환 시 세그먼트 위반으로 인한 GPF를 유발할 수 있는 잠재적 위험 요소였습니다.

## 4. 해결책 및 적용 사항

### 1) `insw` 레지스터 복사 순서 교정
`RDX`가 오염되기 전에 `count` 값을 먼저 `RCX`로 대피시키도록 수정했습니다.
```nasm
insw:
    push rdi
    push rcx
    mov rcx, rdx    ; 1. count를 먼저 복사
    mov dx, di      ; 2. 그 다음 port 번호 설정
    mov rdi, rsi
    cld
    rep insw
    ...
```

### 2) IDT 예외 핸들러 전수 등록
0번부터 31번까지의 모든 예외에 대해 핸들러를 등록하고, 이름을 출력하도록 개선했습니다. 이를 통해 `#UD`와 같은 실제 에러의 정체를 명확히 파악할 수 있게 되었습니다.

### 3) 시스템 안정성 강화 로직 추가
- **FAT32**: 디스크에서 읽어온 `sectors_per_cluster` 값이 0이거나 비정상적일 경우 마운트를 중단하는 방어 코드(Guard Clause)를 추가했습니다.
- **Task System**: 커널 스택 상단(`current_kernel_stack_top`)을 0이 아닌 실제 RSP 값으로 초기화하여 안정성을 높였습니다.

## 5. 결론

이번 문제는 저수준 어셈블리 함수에서의 **레지스터 간섭(Register Interference)**이 메모리 오염으로 이어져 시스템 전체의 불안정성을 초래한 전형적인 사례였습니다. 예외 처리 시스템을 강화함으로써 보이지 않던 에러를 가시화하고, 근본적인 버그를 찾아낼 수 있었습니다.
