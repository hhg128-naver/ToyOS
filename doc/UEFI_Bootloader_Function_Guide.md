# UEFI Bootloader Function Guide (UEFI 부트로더 함수 가이드)

이 문서는 `src/bootloader/uefi/efi_main.c`에서 사용된 주요 UEFI 함수들의 역할과 필요성에 대해 설명합니다.

## 1. 초기화 및 프로토콜 획득 (Initialization & Protocol Setup)

### InitializeLib(ImageHandle, SystemTable)
- **역할**: GNU-EFI 라이브러리의 내부 컨텍스트를 초기화합니다.
- **필요성**: `Print()`, `uefi_call_wrapper()`와 같은 편리한 라이브러리 함수들이 UEFI 시스템 테이블을 참조하여 정상 작동하도록 만듭니다.

### LocateProtocol (&GraphicsOutputProtocol, ...)
- **역할**: 화면 출력을 담당하는 GOP(Graphics Output Protocol)를 찾습니다.
- **필요성**: 커널이 부팅된 후 VGA 텍스트 모드가 아닌 그래픽 모드에서 직접 픽셀을 제어하기 위해 프레임버퍼(Framebuffer) 정보를 얻어야 합니다.

### HandleProtocol (&LoadedImageProtocol, &FileSystemProtocol)
- **역할**: 현재 부트로더가 실행된 장치의 파일 시스템에 접근할 수 있는 핸들을 가져옵니다.
- **필요성**: 동일한 디스크(또는 파티션)에 위치한 커널 바이너리 파일(`kernel`)을 읽어오기 위해 필요합니다.

## 2. 파일 I/O 및 커널 로딩 (File I/O & Kernel Loading)

### OpenVolume / Open
- **역할**: 루트 디렉터리를 열고 특정 파일("kernel")에 대한 핸들을 획득합니다.
- **필요성**: 스토리지에서 커널의 데이터를 메모리로 복사하기 위한 전제 조건입니다.

### Read (ELF Header & Program Headers)
- **역할**: ELF 형식의 헤더 정보를 읽어 메모리 레이아웃을 파악합니다.
- **필요성**: 커널이 실행되기 위해 어떤 주소에 로드되어야 하는지, 엔트리 포인트는 어디인지 확인해야 합니다.

### AllocatePages (AllocateAddress, ...)
- **역할**: 특정 물리 주소 범위의 메모리 페이지를 커널용으로 예약합니다.
- **필요성**: 운영체제 커널은 컴파일 시 지정된 특정 물리 주소에서 실행되어야 하므로, 해당 위치를 UEFI로부터 미리 확보해야 합니다.

## 3. 제어권 이양 (Handover to Kernel)

### GetMemoryMap
- **역할**: 현재 시스템의 전체 메모리 사용 현황(맵)을 가져옵니다.
- **필요성**: `ExitBootServices` 호출 시 시스템 상태가 변하지 않았음을 증명하는 `MapKey`를 얻기 위해, 그리고 커널에 메모리 상태를 전달하기 위해 필요합니다.

### ExitBootServices
- **역할**: UEFI의 부팅 서비스를 종료하고 제어권을 운영체제로 완전히 넘깁니다.
- **필요성**: 커널이 시스템의 모든 자원(CPU, 메모리, 인터럽트 등)을 직접 제어하기 위해 UEFI 펌웨어의 간섭을 중단시키는 필수 과정입니다.

### KernelEntry Call (kmain)
- **역할**: 커널의 진입점으로 점프합니다.
- **필요성**: 부트로더의 임무를 마치고 실제 ToyOS 커널 코드를 실행하기 위함입니다.
