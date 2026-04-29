# VFS 및 FAT32 파일 시스템 구현 상세 계획

이미 구현된 IDE 드라이버를 기반으로, 가상 파일 시스템(VFS) 계층을 도입하고 FAT32 파일 시스템을 지원합니다.

## User Review Required
> [!IMPORTANT]
> - **파일 시스템 제한**: 초기 구현에서는 읽기 전용(Read-only) 기능을 먼저 구현하며, 루트 디렉터리의 파일들만 탐색할 수 있는 수준을 목표로 합니다.
> - **메모리 할당**: VFS 노드 및 디렉터리 엔트리 관리를 위해 기존 커널 힙을 사용합니다.

## Proposed Changes

### 1. 가상 파일 시스템 (VFS) 계층 구현
모든 파일 시스템의 공통 인터페이스를 정의합니다.

#### [NEW] [vfs.h](file:///f:/ToyOS/src/kernel/vfs.h) / [vfs.c](file:///f:/ToyOS/src/kernel/vfs.c)
- `VFS_Node` 구조체 정의 (파일명, 크기, 읽기/탐색 함수 포인터 포함).
- `VFS_Read`, `VFS_ReadDir`, `VFS_FindDir` 시스템 호출 수준의 인터페이스 제공.

### 2. FAT32 파일 시스템 드라이버 구현
디스크 섹터 데이터를 해석하여 파일 정보를 추출합니다.

#### [NEW] [fat32.h](file:///f:/ToyOS/src/kernel/fat32.h) / [fat32.c](file:///f:/ToyOS/src/kernel/fat32.c)
- `FAT32_BPB` 구조체 정의: 부트 섹터 정보 파싱.
- `FAT32_Init()`: 디스크에서 FAT32 구조 확인 및 루트 디렉터리 노드 생성.
- `FAT32_Read()`: 클러스터 체인을 따라가며 파일 데이터 읽기.
- `FAT32_ReadDir()`: 디렉터리 섹터를 읽어 파일 목록 반환.

### 3. 사용자 인터페이스 확장
쉘에서 파일을 다룰 수 있도록 명령어를 추가합니다.

#### [MODIFY] [shell.c](file:///f:/ToyOS/src/kernel/shell.c)
- `ls`: 루트 디렉터리의 파일 리스트 출력.
- `cat`: 특정 파일의 내용을 읽어 화면에 출력.

### 4. 커널 초기화
#### [MODIFY] [kernel.c](file:///f:/ToyOS/src/kernel/kernel.c)
- IDE 초기화 이후 `FAT32_Init()`을 호출하여 기본 파일 시스템을 마운트합니다.

## Verification Plan

### Automated Tests
- `make run-uefi`로 실행하여 `FAT32: Mounted successfully` 메시지 확인.
- `ls` 명령어로 디스크 내 파일(`kernel`, `BOOTX64.EFI` 등) 목록이 올바르게 나오는지 확인.

### Manual Verification
- `cat` 명령어로 작은 텍스트 파일을 읽어 내용이 깨지지 않고 출력되는지 확인.
- 클러스터가 여러 개에 걸쳐 있는 큰 파일의 경우에도 정상적으로 읽히는지 확인.
