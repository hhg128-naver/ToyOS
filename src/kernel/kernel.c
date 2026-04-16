#include "kernel.h"

/*
 * kmain: 커널의 핵심 시작 지점입니다. 
 * boot_info: 부트로더로부터 전달된 시스템 정보를 담고 있는 구조체 포인터입니다.
 */
void kmain(BootInfo *boot_info)
{
    /* 
     * vidptr: 프레임버퍼의 시작 주소(VRAM)입니다. 
     * 32비트 색상(RGBA/BGRA) 값을 직접 써서 화면에 픽셀을 그릴 수 있습니다.
     */
    uint32_t *vidptr = boot_info->framebuffer;
    uint64_t screensize = boot_info->screen_size;

    /* 
     * 1. 배경 초기화: 화면 전체를 남색(Dark Blue)으로 채웁니다. 
     * 이 작업이 성공하면 프레임버퍼 정보가 커널에 잘 전달되었음을 의미합니다.
     */
    for (uint64_t i = 0; i < screensize; i++) {
        vidptr[i] = 0x00000033; // Dark Blue
    }

    /* 
     * 2. 메모리 맵 검증: 전달받은 메모리 맵 정보를 탐색합니다.
     * mmap: 부트로더가 할당해준 메모리 맵 버퍼의 시작 위치입니다.
     * mmap_size: 맵 전체 데이터의 크기(바이트 단위)입니다.
     * descriptor_size: 각 메모리 영역 기술자(Descriptor) 한 개의 크기입니다.
     */
    uint8_t *mmap_ptr = (uint8_t *)boot_info->mmap;
    uint64_t num_entries = boot_info->mmap_size / boot_info->descriptor_size;

    /* 
     * 현재는 텍스트 출력이 안 되므로, 화면 상단에 
     * 메모리 맵 엔트리 개수만큼 흰색 픽셀을 가로로 그어 데이터가 있음을 확인합니다.
     */
    for (uint64_t i = 0; i < num_entries && i < boot_info->horizontal_resolution; i++) {
        vidptr[i] = 0x00FFFFFF; // White Pixel
    }

    /* 
     * 3. 가용 메모리 시각화: 메모리 맵을 순회하며 '가용 메모리(Type 7: Conventional)'를 찾습니다.
     * 가용 메모리 영역의 수만큼 화면 중앙에 연두색 줄을 그어 정보의 정확성을 확인합니다.
     */
    int available_count = 0;
    for (uint64_t i = 0; i < num_entries; i++) {
        // i번째 메모리 기술자의 위치를 계산합니다.
        MemoryDescriptor *desc = (MemoryDescriptor *)(mmap_ptr + (i * boot_info->descriptor_size));
        
        // UEFI Type 7은 운영체제가 자유롭게 사용할 수 있는 Conventional Memory입니다.
        if (desc->type == 7) { 
            available_count++;
            // 가용 메모리 영역 하나당 화면 특정 위치에 연두색 픽셀을 출력합니다.
            if (available_count < 100) {
                vidptr[available_count + (boot_info->horizontal_resolution * 50)] = 0x0000FF00; // Green
            }
        }
    }

    /* 
     * 커널이 종료되지 않도록 무한 루프를 돌립니다. 
     * 향후에는 여기서 스케줄러를 가동하거나 셸 입력을 기다리게 됩니다.
     */
    while(1);
}
