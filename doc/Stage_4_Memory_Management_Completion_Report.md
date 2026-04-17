# ToyOS 4단계 성과 요약 보고서 (Stage 4 Completion Report)

## 1. 개요 (Overview)
본 보고서는 ToyOS 개발 4단계인 **물리/가상 메모리 관리자(PMM/VMM) 구현**의 기술적 성과와 해결된 문제를 요약합니다. 이 단계를 통해 ToyOS는 x86-64 아키텍처의 가상 주소 체계를 완전히 통제하게 되었습니다.

## 2. 주요 구현 내용 (Key Implementations)

### 2.1 물리 메모리 관리자 (Physical Memory Manager, PMM)
- **비트맵 기반 관리 (Bitmap-based Management)**: 4KB 페이지 단위로 메모리 점유 상태를 1비트로 관리합니다.
- **UEFI 메모리 맵 핸드오버 (UEFI Memory Map Handover)**: 부트로더로부터 전달된 메모리 기술자(Descriptor)를 파싱하여 `EfiConventionalMemory` 영역을 탐색하고 관리 대상으로 등록합니다.
- **중요 영역 보호 (Mandatory Protection)**:
    - 커널 코드 및 데이터 영역 보호.
    - 비트맵 자체 저장 공간 보호 (0x200000 위치).
    - BIOS 및 UEFI 예약 영역 충돌 방지.

### 2.2 가상 메모리 관리자 (Virtual Memory Manager, VMM)
- **4단계 페이징 구조 (4-Level Paging Structure)**: PML4, PDPT, PD, PT 테이블을 동적으로 할당하고 관리하는 로직을 구현했습니다.
- **Identity Mapping (1:1 매핑)**: 물리 주소 0~1GB 범위를 가상 주소 0~1GB로 매핑하여 기존 커널 로직과 하드웨어 접근의 연속성을 보장합니다.
- **그래픽 콘솔 보호 (Graphic Console Protection)**: UEFI 프레임버퍼(VRAM)의 물리 주소를 가상 주소 테이블에 포함시켜, 페이징 활성화 후에도 `PrintString` 등 그래픽 출력이 중단되지 않도록 조치했습니다.

### 2.3 시스템 통합 (System Integration)
- **CR3 제어 (CR3 Register Control)**: `asm_utils.asm`에 정의된 `LoadPageTable`을 통해 커널이 생성한 새로운 페이지 테이블을 CPU에 적용했습니다.
- **안정성 확보**: `NULL` 정의 추가 및 인코딩 문제 해결을 통해 빌드 안정성을 강화했습니다.

## 3. 해결된 문제 (Resolved Issues)
- **Triple Fault (무한 재부팅)**: 페이지 테이블 교체 직후 프레임버퍼 주소 미매핑으로 인한 Fault 문제를 해결했습니다.
- **Memory Corruption**: 비트맵 주소 미설정으로 인한 메모리 오염 문제를 비트맵 위치 고정 및 보호 로직으로 해결했습니다.

## 4. 향후 계획 (Next Steps)
- **5단계: 커널 힙(Kernel Heap) 구현**: 슬랩 할당자(Slab Allocator) 또는 리스트 기반 할당자를 구현하여 `kmalloc`/`kfree` 기능을 제공할 예정입니다.
- **6단계: 멀티태스킹(Multitasking)**: 프로세스 제어 블록(PCB) 및 컨텍스트 스위칭(Context Switching) 구현을 준비합니다.

---
**보고서 작성일:** 2026년 4월 17일
**작성자:** Gemini CLI (ToyOS Assistant)
