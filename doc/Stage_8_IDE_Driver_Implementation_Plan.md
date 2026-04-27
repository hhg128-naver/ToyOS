# 하드디스크 IDE 드라이버 구현 계획

ToyOS에서 영구 저장 장치를 관리하고 파일을 읽을 수 있도록 IDE(ATA) 드라이버를 구현합니다. 초기 구현은 단순함을 위해 PIO(Programmed I/O) 방식을 사용하며, LBA28 모드로 0번 채널의 마스터 드라이브를 제어합니다.

## User Review Required
> [!IMPORTANT]
> - **ATA PIO 방식**: 현재 구현하는 방식은 CPU가 직접 데이터를 전송하는 PIO 방식입니다. 이는 DMA(Direct Memory Access)보다 느리지만 초기 단계에서 구현하기 가장 적합합니다.
> - **LBA28 지원**: 현재 계획은 최대 128GB까지 지원 가능한 LBA28 모드를 사용합니다. 추후 대용량 디스크 지원이 필요하면 LBA48로 확장할 수 있습니다.

## Proposed Changes

### 1. 저수준 어셈블리 유틸리티 보완
디스크 컨트롤러와 통신하기 위해 16비트 단위의 포트 I/O 기능이 필요합니다.

#### [MODIFY] [asm_utils.asm](file:///f:/ToyOS/src/kernel/asm_utils.asm)
- `outw`: 16비트 값을 포트로 출력.
- `inw`: 16비트 값을 포트에서 입력.
- `insw`: 대량의 데이터를 포트로부터 메모리 버퍼로 직접 읽기 (`rep insw` 사용).

### 2. IDE 드라이버 구현
ATA 표준 명세에 따라 컨트롤러를 초기화하고 섹터를 읽는 기능을 구현합니다.

#### [NEW] [ide.h](file:///f:/ToyOS/src/kernel/ide.h)
- IDE 관련 상수 정의 (Base Port 0x1F0, Register offsets 등).
- `IDE_Init`, `IDE_ReadSectors` 함수 프로토타입 선언.

#### [NEW] [ide.c](file:///f:/ToyOS/src/kernel/ide.c)
- `IDE_WaitReady()`: 디스크가 명령을 받을 준비가 될 때까지 대기.
- `IDE_Init()`: 드라이브 존재 여부 확인 및 상태 체크.
- `IDE_ReadSectors(uint32_t lba, uint8_t count, void* buffer)`: LBA 주소와 섹터 개수를 지정하여 데이터 로드.

### 3. 커널 초기화 및 통합
커널 부팅 시 디스크 드라이버를 로드합니다.

#### [MODIFY] [kernel.h](file:///f:/ToyOS/src/kernel/kernel.h)
- 새롭게 추가된 I/O 함수(`outw`, `inw`, `insw`)들의 C 프로토타입 추가.

#### [MODIFY] [kernel.c](file:///f:/ToyOS/src/kernel/kernel.c)
- `kmain`에서 `IDE_Init()` 호출 추가.
- 초기화 성공 시 로그 출력.

## Verification Plan

### Automated Tests
- `make run-uefi` 실행 후 부팅 화면에서 `IDE: Primary Master Drive detected` 메시지 확인.
- 디스크 읽기 테스트 코드(임시)를 통해 특정 섹터의 데이터가 정상적으로 버퍼에 담기는지 확인.

### Manual Verification
- QEMU 실행 시 `-hda hdd.img` 옵션이 제대로 설정되어 있는지 확인 (Makefile 점검).
- 추후 구현될 VFS/FAT32 단계에서 실제 파일 내용을 읽어오는 것으로 최종 검증.
