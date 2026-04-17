# 5단계: 인터럽트(PIC) 및 키보드 입력 구현 계획

## 1. 개요 (Overview)
외부 하드웨어 이벤트를 감지하기 위해 인터럽트 컨트롤러(PIC)를 활성화하고, PS/2 키보드로부터 입력을 받아 화면에 출력하는 시스템을 구축합니다.

## 2. 설계 사양 (Design Specification)

### 2.1 PIC (Programmable Interrupt Controller) 설정
- **컨트롤러**: 8259A PIC (Master/Slave 구조)
- **IRQ 매핑 (Offset)**:
    - Master PIC (IRQ 0~7): IDT 벡터 `0x20` (32) ~ `0x27` (39)
    - Slave PIC (IRQ 8~15): IDT 벡터 `0x28` (40) ~ `0x2F` (47)
- **주요 포트**: Master(0x20, 0x21), Slave(0xA0, 0xA1)

### 2.2 키보드 입력 처리
- **IRQ 번호**: IRQ 1 (IDT 벡터 0x21)
- **I/O 포트**: `0x60` (Data Port)
- **처리 흐름**:
    1. 키보드 인터럽트 발생 -> ISR 실행.
    2. 포트 `0x60`에서 스캔 코드(Scan Code) 읽기.
    3. 스캔 코드를 ASCII 문자로 변환.
    4. 화면(그래픽 콘솔)에 출력.
    5. PIC에 EOI(End of Interrupt) 전송.

## 3. 구현 단계 (Phases)

### 1단계: 인터럽트 공통 루틴 및 EOI 정의
- `asm_utils.asm`: 인터럽트 발생 시 레지스터를 백업하고 C 핸들러를 호출하는 `irq_common_stub` 작성.
- `interrupt.c`: PIC에 작업 완료를 알리는 `SendEOI` 함수 추가.

### 2단계: PIC 초기화 (`PIC_Init`)
- `src/kernel/pic.c`, `pic.h` 생성.
- ICW(Initialization Command Words)를 순차적으로 전송하여 PIC 매핑 및 마스크 설정.

### 3단계: 키보드 드라이버 구현
- `src/kernel/keyboard.c`, `keyboard.h` 생성.
- 단순한 영어 쿼티(QWERTY) 레이아웃 매핑 테이블 작성.

### 4단계: 시스템 통합
- `kernel.c`: `PIC_Init()` 및 `Keyboard_Init()` 호출.
- `asm` 명령어 `sti`를 통해 CPU 인터럽트 활성화.

## 4. 검증 시나리오
- [ ] 키보드 아무 키나 눌렀을 때 화면에 해당 문자가 출력되는지 확인.
- [ ] 엔터 키 입력 시 개행(`\n`) 처리가 되는지 확인.
- [ ] 인터럽트 처리 후 시스템이 멈추지 않고 계속 동작하는지 확인.
