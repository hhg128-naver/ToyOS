#include <efi.h>
#include <efilib.h>

typedef struct {
    unsigned char e_ident[16];
    UINT16 e_type;
    UINT16 e_machine;
    UINT32 e_version;
    UINT64 e_entry;
    UINT64 e_phoff;
    UINT64 e_shoff;
    UINT32 e_flags;
    UINT16 e_ehsize;
    UINT16 e_phentsize;
    UINT16 e_phnum;
} Elf64_Ehdr;

typedef struct {
    UINT32 p_type;
    UINT32 p_flags;
    UINT64 p_offset;
    UINT64 p_vaddr;
    UINT64 p_paddr;
    UINT64 p_filesz;
    UINT64 p_memsz;
    UINT64 p_align;
} Elf64_Phdr;

typedef struct {
    unsigned int *framebuffer;           // 프레임버퍼 시작 주소 (VRAM)
    unsigned long screen_size;           // 전체 화면 크기 (픽셀 수)
    unsigned int horizontal_resolution;  // 가로 해상도
    unsigned int vertical_resolution;    // 세로 해상도
    VOID *mmap;                          // UEFI 메모리 맵 버퍼 포인터
    UINTN mmap_size;                     // 메모리 맵 전체 크기
    UINTN descriptor_size;               // 메모리 맵 각 엔트리(Descriptor)의 크기
    VOID *rsdp;                          // ACPI RSDP 주소
} BootInfo;

typedef void (*KernelEntry)(BootInfo *);

EFI_STATUS
EFIAPI
efi_main (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    EFI_STATUS Status;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop;
    EFI_LOADED_IMAGE *LoadedImage;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem;
    EFI_FILE *Root, *KernelFile;

    /*
     * InitializeLib: GNU-EFI 라이브러리를 초기화합니다.
     * UEFI 시스템 테이블(SystemTable)과 이미지 핸들(ImageHandle)을 설정하여, 
     * 이후 Print()와 같은 라이브러리 함수들이 올바르게 작동하도록 컨텍스트를 구성합니다.
     */
    InitializeLib(ImageHandle, SystemTable);

    Print(L"ToyOS: ELF64 Handover Start\n");

    /*
     * LocateProtocol (GOP): 그래픽 출력 프로토콜(Graphics Output Protocol)을 찾습니다.
     * 커널이 부팅된 후 화면에 직접 픽셀을 그릴 수 있도록 프레임버퍼(Framebuffer)의 
     * 시작 주소와 해상도 정보를 얻기 위해 반드시 필요합니다.
     */
    Status = uefi_call_wrapper(SystemTable->BootServices->LocateProtocol, 3, 
                               &GraphicsOutputProtocol, NULL, (VOID **)&Gop);
    if (EFI_ERROR(Status)) return Status;

    /*
     * HandleProtocol (LoadedImage): 현재 실행 중인 부트로더 이미지 정보를 가져옵니다.
     * 이 부트로더가 실행된 장치(DeviceHandle)를 알아내어, 동일한 위치에 있는 
     * 커널 파일을 찾기 위한 기반 정보를 얻습니다.
     */
    Status = uefi_call_wrapper(SystemTable->BootServices->HandleProtocol, 3,
                               ImageHandle, &LoadedImageProtocol, (VOID **)&LoadedImage);
    
    /*
     * HandleProtocol (FileSystem): 파일 시스템 접근을 위한 프로토콜을 가져옵니다.
     * 부팅 장치 내의 파일(커널 바이너리 등)을 열고 읽기 위해 필요합니다.
     */
    Status = uefi_call_wrapper(SystemTable->BootServices->HandleProtocol, 3,
                               LoadedImage->DeviceHandle, &FileSystemProtocol, (VOID **)&FileSystem);
    
    /*
     * OpenVolume: 부팅 장치의 루트 볼륨(Root Volume)을 엽니다.
     * 파일 경로를 탐색하기 위한 최상위 디렉터리 핸들을 확보합니다.
     * 볼륨을 열고 커널 바이너리 파일을 읽기 전용 모드로 엽니다.
     */
    Status = uefi_call_wrapper(FileSystem->OpenVolume, 2, FileSystem, &Root);
    
    /*
     * Open (kernel): "kernel"이라는 이름의 파일을 읽기 모드로 엽니다.
     * 실제 운영체제 커널 코드가 담긴 바이너리 파일을 메모리에 로드하기 위함입니다.
     */
    Status = uefi_call_wrapper(Root->Open, 5, Root, &KernelFile, L"kernel", EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status)) {
        Print(L"Kernel not found!\n");
        return Status;
    }

    /*
     * ELF Header Read: ELF 파일 형식의 헤더를 읽습니다.
     * 커널 바이너리의 엔트리 포인트(Entry Point) 위치와 메모리에 로드해야 할 
     * 세그먼트(Segment) 정보를 파악하기 위해 필수적인 단계입니다.
     */
    Elf64_Ehdr Header;
    UINTN HeaderSize = sizeof(Elf64_Ehdr);
    uefi_call_wrapper(KernelFile->Read, 3, KernelFile, &HeaderSize, &Header);

    /*
     * Program Headers: ELF의 프로그램 헤더들을 메모리에 로드합니다.
     * 각 헤더는 커널의 어떤 부분을 메모리의 어디에 로드해야 하는지 명시합니다.
     */
    Elf64_Phdr *Phdrs;
    UINTN PhdrSize = Header.e_phnum * Header.e_phentsize;
    uefi_call_wrapper(SystemTable->BootServices->AllocatePool, 3, EfiLoaderData, PhdrSize, (VOID **)&Phdrs);
    uefi_call_wrapper(KernelFile->SetPosition, 2, KernelFile, Header.e_phoff);
    uefi_call_wrapper(KernelFile->Read, 3, KernelFile, &PhdrSize, Phdrs);

    /*
     * PT_LOAD 타입의 세그먼트들을 ELF에 명시된 특정 물리 주소로 로드합니다.
     * AllocatePages를 통해 주소를 예약하고, Read를 통해 바이너리 데이터를 복사합니다.
     */
    for (int i = 0; i < Header.e_phnum; i++) {
        if (Phdrs[i].p_type == 1) { // PT_LOAD: 로드가 필요한 실제 코드나 데이터 세그먼트
            EFI_PHYSICAL_ADDRESS Addr = Phdrs[i].p_paddr;
            UINTN Pages = (Phdrs[i].p_memsz + 0xFFF) / 0x1000;
            
            /*
             * AllocatePages: 커널이 요구하는 특정 물리 주소(p_paddr)에 메모리를 예약합니다.
             * ELF에 명시된 고정된 주소에 커널이 위치해야 정상적으로 실행될 수 있습니다.
             */
            Status = uefi_call_wrapper(SystemTable->BootServices->AllocatePages, 4,
                                       AllocateAddress, EfiLoaderData, Pages, &Addr);
            
            if (EFI_ERROR(Status)) {
                Print(L"Failed to allocate memory at 0x%lx. Error: %r\n", Phdrs[i].p_paddr, Status);
                return Status;
            }

            /*
             * Read (Segment Content): 파일에서 실제 바이너리 데이터를 읽어 할당된 메모리에 복사합니다.
             */
            uefi_call_wrapper(KernelFile->SetPosition, 2, KernelFile, Phdrs[i].p_offset);
            UINTN Size = Phdrs[i].p_filesz;
            uefi_call_wrapper(KernelFile->Read, 3, KernelFile, &Size, (VOID *)Addr);

            /* BSS 영역과 같이 파일 크기보다 메모리 크기가 큰 경우 나머지를 0으로 초기화합니다. */
            if (Phdrs[i].p_memsz > Phdrs[i].p_filesz) {
                SetMem((VOID *)(Addr + Phdrs[i].p_filesz), Phdrs[i].p_memsz - Phdrs[i].p_filesz, 0);
            }
        }
    }

    /*
     * BootInfo 초기화: 커널이 필요로 하는 정보를 구조체에 담습니다.
     * 이 시점에는 아직 메모리 맵 정보가 확정되지 않았습니다.
     */
    BootInfo *BInfo;
    uefi_call_wrapper(SystemTable->BootServices->AllocatePool, 3, EfiLoaderData, sizeof(BootInfo), (VOID **)&BInfo);
    BInfo->framebuffer = (unsigned int *)Gop->Mode->FrameBufferBase;
    BInfo->horizontal_resolution = Gop->Mode->Info->HorizontalResolution;
    BInfo->vertical_resolution = Gop->Mode->Info->VerticalResolution;
    BInfo->screen_size = (unsigned long)BInfo->horizontal_resolution * BInfo->vertical_resolution;

    /*
     * ACPI RSDP 탐색: EFI Configuration Table에서 ACPI 테이블 포인터를 찾습니다.
     * ACPI 2.0+ XSDP(GUID: 8868e871-...)를 우선 탐색하고,
     * 없으면 ACPI 1.0 RSDP(GUID: eb9d2d30-...)를 사용합니다.
     */
    BInfo->rsdp = NULL;
    EFI_GUID Acpi20Guid = {0x8868e871, 0xe4f1, 0x11d3, {0xbc, 0x22, 0x00, 0x80, 0xc7, 0x3c, 0x88, 0x81}};
    EFI_GUID Acpi10Guid = {0xeb9d2d30, 0x2d88, 0x11d3, {0x9a, 0x16, 0x00, 0x90, 0x27, 0x3f, 0xc1, 0x4d}};
    for (UINTN i = 0; i < SystemTable->NumberOfTableEntries; i++)
    {
        EFI_CONFIGURATION_TABLE *entry = &SystemTable->ConfigurationTable[i];
        if (CompareGuid(&entry->VendorGuid, &Acpi20Guid) == 0)
        {
            BInfo->rsdp = entry->VendorTable;
            Print(L"Found ACPI 2.0+ RSDP at 0x%lx\n", (UINT64)entry->VendorTable);
            break;
        }
        if (CompareGuid(&entry->VendorGuid, &Acpi10Guid) == 0 && BInfo->rsdp == NULL)
        {
            BInfo->rsdp = entry->VendorTable;
            Print(L"Found ACPI 1.0 RSDP at 0x%lx\n", (UINT64)entry->VendorTable);
        }
    }

    // --- Critical Section: GetMemoryMap and ExitBootServices ---
    UINTN MemoryMapSize = 0, MapKey, DescriptorSize;
    UINT32 DescriptorVersion;
    EFI_MEMORY_DESCRIPTOR *MemoryMap = NULL;

    Print(L"Jumping to Kernel at 0x%lx...\n", Header.e_entry);

    /*
     * 1. GetMemoryMap (크기 확인): 현재 메모리 맵의 크기를 확인합니다.
     * UEFI는 수시로 메모리 상태가 변하므로, 실제 데이터 획득 전 넉넉한 버퍼를 할당해야 합니다.
     */
    uefi_call_wrapper(SystemTable->BootServices->GetMemoryMap, 5, &MemoryMapSize, NULL, &MapKey, &DescriptorSize, &DescriptorVersion);
    MemoryMapSize += 2 * DescriptorSize; // 여유 공간 확보
    uefi_call_wrapper(SystemTable->BootServices->AllocatePool, 3, EfiLoaderData, MemoryMapSize, (VOID **)&MemoryMap);

    /*
     * 2. GetMemoryMap (데이터 획득): 현재 시스템의 전체 물리 메모리 맵을 가져옵니다.
     * 이 데이터는 나중에 운영체제 커널의 메모리 관리자(PMM)가 기초 자료로 사용하게 됩니다.
     * 이 정보는 ExitBootServices 호출 시 현재 시스템의 상태가 최신인지 검증하는 용도로 사용됩니다.
     */
    Status = uefi_call_wrapper(SystemTable->BootServices->GetMemoryMap, 5, &MemoryMapSize, MemoryMap, &MapKey, &DescriptorSize, &DescriptorVersion);
    if (EFI_ERROR(Status)) return Status;

    /* 커널에 넘겨줄 BootInfo 구조체에 메모리 맵 포인터와 크기 정보를 저장합니다. */
    BInfo->mmap = (VOID *)MemoryMap;
    BInfo->mmap_size = MemoryMapSize;
    BInfo->descriptor_size = DescriptorSize;

    /*
     * 3. ExitBootServices: UEFI의 부팅 서비스 환경을 완전히 종료합니다.
     * 성공 시 하드웨어 제어권이 펌웨어에서 커널로 완전히 넘어갑니다.
     * MapKey는 GetMemoryMap 호출 이후 메모리 상태가 변하지 않았음을 보장합니다.
     */
    Status = uefi_call_wrapper(SystemTable->BootServices->ExitBootServices, 2, ImageHandle, MapKey);

    if (EFI_ERROR(Status)) {
        /*
         * 만약 실패했다면(보통 GetMemoryMap 호출 이후의 동작으로 인해 맵이 변경된 경우), 
         * 맵 정보를 다시 가져와서 재시도해야 합니다.
         */
        uefi_call_wrapper(SystemTable->BootServices->GetMemoryMap, 5, &MemoryMapSize, MemoryMap, &MapKey, &DescriptorSize, &DescriptorVersion);
        
        // 갱신된 맵 정보와 크기를 BootInfo에 다시 업데이트합니다.
        BInfo->mmap = (VOID *)MemoryMap;
        BInfo->mmap_size = MemoryMapSize;
        BInfo->descriptor_size = DescriptorSize;

        Status = uefi_call_wrapper(SystemTable->BootServices->ExitBootServices, 2, ImageHandle, MapKey);
    }

    if (EFI_ERROR(Status)) return Status;

    /*
     * 4. Kernel Jump: ELF 헤더에 기록된 엔트리 포인트로 제어권을 넘깁니다.
     * 이제부터는 UEFI 서비스(Print 등)를 사용할 수 없으며, 커널이 시스템의 주인이 됩니다.
     */
    KernelEntry kmain = (KernelEntry)Header.e_entry;
    kmain(BInfo);

    return EFI_SUCCESS;
}
