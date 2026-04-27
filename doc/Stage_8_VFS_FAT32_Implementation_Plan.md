# 8단계: 가상 파일 시스템(VFS) 및 FAT32 지원 계획

ToyOS에서 영구 저장 장치를 관리하고 파일을 읽을 수 있도록 가상 파일 시스템(VFS)과 FAT32 드라이버를 구현합니다.

## User Review Required
> [!IMPORTANT]
> 이 단계에서는 ATA PIO 모드를 사용하여 디스크에 접근합니다. 이는 현대적인 AHCI보다 속도는 느리지만 구현이 단순하여 초기 커널 개발에 적합합니다. 
> QEMU 실행 시 가상 디스크 이미지가 필요하며, 현재 `make run-uefi`에서 사용하는 `iso` 폴더를 FAT32 디스크로 간주하여 처리할 예정입니다.

## Proposed Changes

### 1. 저수준 I/O 및 디스크 드라이버 구현
디스크의 섹터(Sector) 단위로 데이터를 읽고 쓸 수 있는 기반을 마련합니다.

#### [MODIFY] [asm_utils.asm](file:///f:/ToyOS/src/kernel/asm_utils.asm)
- `outw`, `inw`: 16비트 I/O 포트 조작 함수 추가.
- `insw`: 포트로부터 메모리로 대량의 데이터를 읽어오는 문자열 입력 함수 추가 (디스크 읽기 가속).

#### [NEW] [ide.h](file:///f:/ToyOS/src/kernel/ide.h) / [ide.c](file:///f:/ToyOS/src/kernel/ide.c)
- `IDE_Init()`: ATA 컨트롤러 및 마스터 드라이브 확인.
- `IDE_ReadSectors(uint32_t lba, uint8_t count, void* buffer)`: LBA 주소를 사용하여 섹터 읽기.

### 2. 가상 파일 시스템 (VFS) 계층 설계
다양한 파일 시스템(현재는 FAT32만 고려)을 추상화하여 동일한 인터페이스로 접근할 수 있게 합니다.

#### [NEW] [vfs.h](file:///f:/ToyOS/src/kernel/vfs.h) / [vfs.c](file:///f:/ToyOS/src/kernel/vfs.c)
- `VFS_Node` 구조체: 파일명, 크기, 속성, 함수 포인터(read, readdir 등) 포함.
- `VFS_Mount()`, `VFS_Open()`, `VFS_Read()`, `VFS_ReadDir()` 인터페이스 정의.

### 3. FAT32 파일 시스템 드라이버 구현
디스크 상의 FAT32 구조를 파싱하여 파일 정보를 추출합니다.

#### [NEW] [fat32.h](file:///f:/ToyOS/src/kernel/fat32.h) / [fat32.c](file:///f:/ToyOS/src/kernel/fat32.c)
- FAT32 BPB(BIOS Parameter Block) 파싱.
- Cluster Chain 추적 및 섹터 주소 계산.
- 디렉터리 엔트리(Directory Entry) 파싱.

### 4. 사용자 인터페이스 및 통합

#### [MODIFY] [shell.c](file:///f:/ToyOS/src/kernel/shell.c)
- `ls`: 현재 디렉터리의 파일 목록 출력 명령어 추가.
- `cat [filename]`: 파일 내용을 읽어서 출력하는 명령어 추가.

#### [MODIFY] [kernel.c](file:///f:/ToyOS/src/kernel/kernel.c)
- IDE 및 VFS/FAT32 초기화 로직 추가.

## Verification Plan

### Automated Tests
- `make run-uefi`로 실행하여 부팅 로그에 "IDE Drive Detected" 메시지 확인.
- 쉘에서 `ls` 실행 시 `iso` 폴더에 담긴 파일(kernel, efi 등)들이 리스트업되는지 확인.

### Manual Verification
- `cat` 명령어로 특정 텍스트 파일의 내용을 정상적으로 읽어오는지 확인.
- 존재하지 않는 파일 접근 시 에러 처리 확인.
