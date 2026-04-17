# ToyOS GDT/IDT Implementation Plan (3단계: 프로세서 기초 환경 설정)

이 문서는 ToyOS의 64비트 모드 운영을 위한 기초 환경인 GDT와 IDT의 구현 계획을 담고 있습니다.

## 1. GDT (Global Descriptor Table) 설정
UEFI가 설정해준 기본 GDT가 있지만, 커널의 제어권을 완전히 확보하기 위해 자체 GDT를 구축합니다.

### 세그먼트 구성 (Flat Memory Model)
- **Index 0**: NULL Descriptor
- **Index 1**: Kernel Code Segment (64-bit, Ring 0)
- **Index 2**: Kernel Data Segment (64-bit, Ring 0)
- **Index 3**: User Code Segment (추후 사용자 모드 대비)
- **Index 4**: User Data Segment (추후 사용자 모드 대비)
- **Index 5**: TSS (Task State Segment) - 스택 스위칭을 위해 필요

## 2. IDT (Interrupt Descriptor Table) 설정
시스템 예외(Exception) 및 하드웨어 인터럽트를 처리하기 위한 테이블을 설정합니다.

### 주요 예외 처리 (Exceptions 0-31)
- #DE (Divide Error)
- #BP (Breakpoint)
- #UD (Invalid Opcode)
- #GP (General Protection Fault)
- #PF (Page Fault)

### IDT Entry 구조 (64-bit)
- 64비트 모드에서는 엔트리 크기가 16바이트로 확장됩니다.
- ISR(Interrupt Service Routine) 주소는 64비트 전체를 수용해야 합니다.

## 3. 구현 단계
1. `src/kernel/gdt.c`, `gdt.h`: GDT 구조체 및 초기화 로직 구현.
2. `src/kernel/idt.c`, `idt.h`: IDT 구조체 및 초기화 로직 구현.
3. `src/kernel/asm_utils.asm` (또는 인라인 어셈블리): `lgdt`, `lidt` 호출 및 ISR 래퍼 구현.
4. `kmain`에서 초기화 함수 호출 및 테스트.

## 4. 검증 방법
- `int 3` (Breakpoint) 명령어를 사용하여 인터럽트 핸들러가 정상적으로 호출되는지 확인.
- 예외 발생 시 화면에 에러 메시지와 함께 레지스터 상태 출력.
