#include "ide.h"
#include "kernel.h"
#include <stdio.h>

/**
 * IDE 상태가 Busy가 아닐 때까지 대기
 */
static void IDE_WaitReady() {
    while (inb(ATA_PRIMARY_STATUS) & ATA_STATUS_BSY);
}

/**
 * IDE 상태가 데이터 요청(DRQ) 상태가 될 때까지 대기
 */
static void IDE_WaitDRQ() {
    while (!(inb(ATA_PRIMARY_STATUS) & ATA_STATUS_DRQ));
}

/**
 * IDE 컨트롤러 초기화 및 마스터 드라이브 확인
 */
void IDE_Init() {
    // Primary Master 선택 (0xE0: Master, LBA Mode)
    outb(ATA_PRIMARY_DRIVE, 0xE0);
    
    // 400ns 이상의 지연을 위해 상태 레지스터를 4번 읽음
    for(int i=0; i<4; i++) inb(ATA_PRIMARY_STATUS);

    uint8_t status = inb(ATA_PRIMARY_STATUS);
    
    // 드라이브가 존재하지 않으면 0xFF가 반환됨
    if (status == 0xFF) {
        kPrintf("IDE: No drive detected on Primary Master.\n");
        return;
    }
    
    kPrintf("IDE: Primary Master Drive detected (Status: 0x%02x).\n", status);
}

/**
 * LBA28 모드로 섹터 읽기 (PIO 방식)
 * @param lba: 28비트 LBA 주소
 * @param count: 읽을 섹터 수 (1~255, 0이면 256)
 * @param buffer: 데이터를 저장할 버퍼
 */
void IDE_ReadSectors(uint32_t lba, uint8_t count, void* buffer) {
    IDE_WaitReady();
    
    // 드라이브 선택 및 LBA 상위 4비트 설정 (0xE0: Master, LBA Mode)
    outb(ATA_PRIMARY_DRIVE, 0xE0 | ((lba >> 24) & 0x0F));
    
    // 포트 지연
    for(int i=0; i<4; i++) inb(ATA_PRIMARY_STATUS);

    // 읽을 섹터 수 및 LBA 하위 24비트 설정
    outb(ATA_PRIMARY_SECCOUNT, count);
    outb(ATA_PRIMARY_LBA_LOW, (uint8_t)lba);
    outb(ATA_PRIMARY_LBA_MID, (uint8_t)(lba >> 8));
    outb(ATA_PRIMARY_LBA_HIGH, (uint8_t)(lba >> 16));
    
    // 섹터 읽기 명령 전송
    outb(ATA_PRIMARY_COMMAND, ATA_CMD_READ_PIO);
    
    uint16_t* ptr = (uint16_t*)buffer;
    uint32_t sectors_to_read = (count == 0) ? 256 : count;
    
    for (uint32_t i = 0; i < sectors_to_read; i++) {
        // 매 섹터 읽기 전에 Busy 상태가 끝날 때까지 대기
        IDE_WaitReady();
        // 데이터 준비(DRQ) 확인
        IDE_WaitDRQ();
        
        // 한 섹터(256워드 = 512바이트)를 포트로부터 메모리로 복사
        insw(ATA_PRIMARY_DATA, ptr, 256);
        ptr += 256;
    }
}
