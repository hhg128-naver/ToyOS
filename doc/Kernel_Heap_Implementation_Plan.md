# 커널 힙(Kernel Heap) 구현 계획 (Stage 4 Extension)

## 1. 개요 (Overview)
현재 ToyOS는 PMM(물리 메모리)과 VMM(가상 메모리)을 통해 페이지 단위(4KB)의 메모리 관리가 가능합니다. 하지만 커널 내부에서 작은 크기의 동적 메모리 할당(`kmalloc`)이 필요하므로, 이를 처리할 **바이트 단위 할당자**를 구현합니다.

## 2. 설계 사양 (Design Specification)

### 2.1 힙 영역 정의
- **가상 주소 시작**: `0xFFFF800001000000` (Higher-half를 고려한 설계 또는 현재 Identity Mapping 영역인 `0x01000000` 사용 논의 필요)
- **초기 결정**: 현재 시스템의 단순성을 위해 Identity Mapping 영역 내인 `0x01000000` (16MB 지점)부터 시작하여 16MB 공간을 확보합니다.
- **물리 메모리 확보**: 힙 영역 초기화 시 `PMM_AllocPage`를 통해 실제 물리 페이지를 할당받아 `VMM_MapPage`로 연결합니다.

### 2.2 할당 알고리즘: Free List
- **구조**: 연결 리스트(Linked List)를 사용하여 가용 메모리 블록을 관리합니다.
- **메타데이터 (Header)**:
  ```c
  typedef struct HeapBlock {
      uint64_t size;           // 실제 데이터 크기 (헤더 제외)
      struct HeapBlock* next;  // 다음 블록 포인터
      int is_free;             // 1: 가용, 0: 사용 중
  } HeapBlock;
  ```
- **할당 전략**: First-fit (조건에 맞는 첫 번째 블록 선택).
- **최적화**: 블록 분할(Splitting) 및 인접 가용 블록 병합(Coalescing).

## 3. 구현 단계 (Phases)

### 1단계: 헤더 및 기초 구조 정의
- `src/kernel/heap.h` 생성: `HeapBlock` 구조체 및 `kmalloc`, `kfree` 선언.
- `src/kernel/heap.c` 생성: 전역 힙 시작 포인터 정의.

### 2단계: 힙 초기화 (`Heap_Init`)
- 16MB 주소부터 필요한 만큼의 페이지를 PMM에서 할당받아 VMM에 매핑.
- 전체 영역을 하나의 거대한 'Free Block'으로 설정하여 리스트 시작점으로 지정.

### 3단계: `kmalloc` 구현
- 리스트를 순회하며 요청된 `size + sizeof(HeapBlock)`보다 큰 가용 블록 탐색.
- 블록 발견 시 남는 공간이 충분하면 새로운 블록으로 분할.

### 4단계: `kfree` 및 병합 구현
- 대상 블록의 `is_free`를 1로 설정.
- 다음 블록이 존재하고 가용 상태라면 두 블록의 `size`를 합쳐 병합.

### 5단계: 커널 통합 및 검증
- `kernel.c`에서 `Heap_Init()` 호출.
- 문자열 복사 또는 구조체 동적 할당 테스트 수행.

## 4. 기대 효과
- 다양한 크기의 데이터를 메모리 낭비 없이 동적으로 관리 가능.
- 향후 프로세스/스레드 생성 시 필요한 PCB(Process Control Block) 할당의 기반이 됨.
