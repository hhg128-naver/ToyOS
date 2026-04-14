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
    unsigned int *framebuffer;
    unsigned long screen_size;
    unsigned int horizontal_resolution;
    unsigned int vertical_resolution;
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
    InitializeLib(ImageHandle, SystemTable);

    Print(L"ToyOS: ELF64 Handover Start\n");

    Status = uefi_call_wrapper(SystemTable->BootServices->LocateProtocol, 3, 
                               &GraphicsOutputProtocol, NULL, (VOID **)&Gop);
    if (EFI_ERROR(Status)) return Status;

    Status = uefi_call_wrapper(SystemTable->BootServices->HandleProtocol, 3,
                               ImageHandle, &LoadedImageProtocol, (VOID **)&LoadedImage);
    Status = uefi_call_wrapper(SystemTable->BootServices->HandleProtocol, 3,
                               LoadedImage->DeviceHandle, &FileSystemProtocol, (VOID **)&FileSystem);
    Status = uefi_call_wrapper(FileSystem->OpenVolume, 2, FileSystem, &Root);
    Status = uefi_call_wrapper(Root->Open, 5, Root, &KernelFile, L"kernel", EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status)) {
        Print(L"Kernel not found!\n");
        return Status;
    }

    Elf64_Ehdr Header;
    UINTN HeaderSize = sizeof(Elf64_Ehdr);
    uefi_call_wrapper(KernelFile->Read, 3, KernelFile, &HeaderSize, &Header);

    Elf64_Phdr *Phdrs;
    UINTN PhdrSize = Header.e_phnum * Header.e_phentsize;
    uefi_call_wrapper(SystemTable->BootServices->AllocatePool, 3, EfiLoaderData, PhdrSize, (VOID **)&Phdrs);
    uefi_call_wrapper(KernelFile->SetPosition, 2, KernelFile, Header.e_phoff);
    uefi_call_wrapper(KernelFile->Read, 3, KernelFile, &PhdrSize, Phdrs);

    for (int i = 0; i < Header.e_phnum; i++) {
        if (Phdrs[i].p_type == 1) { // PT_LOAD
            EFI_PHYSICAL_ADDRESS Addr = Phdrs[i].p_paddr;
            UINTN Pages = (Phdrs[i].p_memsz + 0xFFF) / 0x1000;
            
            Status = uefi_call_wrapper(SystemTable->BootServices->AllocatePages, 4,
                                       AllocateAddress, EfiLoaderData, Pages, &Addr);
            
            if (EFI_ERROR(Status)) {
                Print(L"Failed to allocate memory at 0x%lx. Error: %r\n", Phdrs[i].p_paddr, Status);
                return Status;
            }

            uefi_call_wrapper(KernelFile->SetPosition, 2, KernelFile, Phdrs[i].p_offset);
            UINTN Size = Phdrs[i].p_filesz;
            uefi_call_wrapper(KernelFile->Read, 3, KernelFile, &Size, (VOID *)Addr);

            if (Phdrs[i].p_memsz > Phdrs[i].p_filesz) {
                SetMem((VOID *)(Addr + Phdrs[i].p_filesz), Phdrs[i].p_memsz - Phdrs[i].p_filesz, 0);
            }
        }
    }

    BootInfo *BInfo;
    uefi_call_wrapper(SystemTable->BootServices->AllocatePool, 3, EfiLoaderData, sizeof(BootInfo), (VOID **)&BInfo);
    BInfo->framebuffer = (unsigned int *)Gop->Mode->FrameBufferBase;
    BInfo->horizontal_resolution = Gop->Mode->Info->HorizontalResolution;
    BInfo->vertical_resolution = Gop->Mode->Info->VerticalResolution;
    BInfo->screen_size = (unsigned long)BInfo->horizontal_resolution * BInfo->vertical_resolution;

    // --- Critical Section: GetMemoryMap and ExitBootServices ---
    UINTN MemoryMapSize = 0, MapKey, DescriptorSize;
    UINT32 DescriptorVersion;
    EFI_MEMORY_DESCRIPTOR *MemoryMap = NULL;

    Print(L"Jumping to Kernel at 0x%lx...\n", Header.e_entry);

    // 1. 필요한 버퍼 크기 확인
    uefi_call_wrapper(SystemTable->BootServices->GetMemoryMap, 5, &MemoryMapSize, NULL, &MapKey, &DescriptorSize, &DescriptorVersion);
    MemoryMapSize += 2 * DescriptorSize;
    uefi_call_wrapper(SystemTable->BootServices->AllocatePool, 3, EfiLoaderData, MemoryMapSize, (VOID **)&MemoryMap);

    // 2. 최종 Memory Map 획득
    Status = uefi_call_wrapper(SystemTable->BootServices->GetMemoryMap, 5, &MemoryMapSize, MemoryMap, &MapKey, &DescriptorSize, &DescriptorVersion);
    if (EFI_ERROR(Status)) return Status;

    // 3. 즉시 ExitBootServices 호출 (사이에 어떠한 Boot Service 호출도 없어야 함)
    Status = uefi_call_wrapper(SystemTable->BootServices->ExitBootServices, 2, ImageHandle, MapKey);

    if (EFI_ERROR(Status)) {
        // 만약 실패하면 한 번 더 시도 (MapKey 갱신을 위해 GetMemoryMap 다시 호출)
        uefi_call_wrapper(SystemTable->BootServices->GetMemoryMap, 5, &MemoryMapSize, MemoryMap, &MapKey, &DescriptorSize, &DescriptorVersion);
        Status = uefi_call_wrapper(SystemTable->BootServices->ExitBootServices, 2, ImageHandle, MapKey);
    }

    if (EFI_ERROR(Status)) return Status;

    // 4. 커널 점프! (이제 UEFI 서비스 사용 불가)
    KernelEntry kmain = (KernelEntry)Header.e_entry;
    kmain(BInfo);

    return EFI_SUCCESS;
}
