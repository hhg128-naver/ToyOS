#ifndef IDE_H
#define IDE_H

#include <stdint.h>

/**
 * ATA Primary Channel Registers
 */
#define ATA_PRIMARY_DATA         0x1F0
#define ATA_PRIMARY_ERR          0x1F1
#define ATA_PRIMARY_FEATURES     0x1F1
#define ATA_PRIMARY_SECCOUNT     0x1F2
#define ATA_PRIMARY_LBA_LOW      0x1F3
#define ATA_PRIMARY_LBA_MID      0x1F4
#define ATA_PRIMARY_LBA_HIGH     0x1F5
#define ATA_PRIMARY_DRIVE        0x1F6
#define ATA_PRIMARY_STATUS       0x1F7
#define ATA_PRIMARY_COMMAND      0x1F7
#define ATA_PRIMARY_CTRL         0x3F6

/**
 * ATA Status Register Bits
 */
#define ATA_STATUS_BSY  0x80  // Busy
#define ATA_STATUS_DRDY 0x40  // Drive Ready
#define ATA_STATUS_DF   0x20  // Drive Fault
#define ATA_STATUS_DSC  0x10  // Drive Seek Complete
#define ATA_STATUS_DRQ  0x08  // Data Request (ready to transfer data)
#define ATA_STATUS_CORR 0x04  // Corrected Data
#define ATA_STATUS_IDX  0x02  // Index
#define ATA_STATUS_ERR  0x01  // Error

/**
 * ATA Commands
 */
#define ATA_CMD_READ_PIO          0x20
#define ATA_CMD_READ_PIO_EXT      0x24
#define ATA_CMD_WRITE_PIO         0x30
#define ATA_CMD_WRITE_PIO_EXT     0x34
#define ATA_CMD_CACHE_FLUSH       0xE7
#define ATA_CMD_CACHE_FLUSH_EXT   0xEA
#define ATA_CMD_PACKET            0xA0
#define ATA_CMD_IDENTIFY          0xEC

#ifdef __cplusplus
extern "C" {
#endif

/**
 * IDE 드라이버 초기화
 */
void IDE_Init();

/**
 * 섹터 읽기 (LBA28 PIO 모드)
 * @param lba: 28비트 LBA 주소
 * @param count: 읽을 섹터 수 (0이면 256섹터)
 * @param buffer: 데이터를 저장할 버퍼 (최소 count * 512 바이트)
 */
void IDE_ReadSectors(uint32_t lba, uint8_t count, void* buffer);

#ifdef __cplusplus
}
#endif

#endif
