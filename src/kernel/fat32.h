#ifndef FAT32_H
#define FAT32_H

#include <stdint.h>
#include "vfs.h"

/**
 * FAT32 Boot Sector (BPB) 구조체
 */
typedef struct {
    uint8_t  jmp[3];
    char     oem_id[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entry_count;
    uint16_t total_sectors_16;
    uint8_t  media_type;
    uint16_t sectors_per_fat_16;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    
    // Extended BPB (FAT32 전용)
    uint32_t sectors_per_fat_32;
    uint16_t ext_flags;
    uint16_t fat_version;
    uint32_t root_cluster;
    uint16_t fs_info_sector;
    uint16_t backup_boot_sector;
    uint8_t  reserved[12];
    uint8_t  drive_num;
    uint8_t  reserved1;
    uint8_t  boot_sig;
    uint32_t volume_id;
    char     volume_label[11];
    char     fs_type[8];
} __attribute__((packed)) FAT32_BPB;

/**
 * FAT32 디렉터리 엔트리 구조체 (8.3 형식)
 */
typedef struct {
    char     name[8];
    char     ext[3];
    uint8_t  attr;
    uint8_t  reserved;
    uint8_t  create_time_tenth;
    uint16_t create_time;
    uint16_t create_date;
    uint16_t last_access_date;
    uint16_t cluster_high;
    uint16_t write_time;
    uint16_t write_date;
    uint16_t cluster_low;
    uint32_t size;
} __attribute__((packed)) FAT32_DirEntry;

/**
 * FAT32 속성 플래그
 */
#define FAT32_ATTR_READ_ONLY 0x01
#define FAT32_ATTR_HIDDEN    0x02
#define FAT32_ATTR_SYSTEM    0x04
#define FAT32_ATTR_VOLUME_ID 0x08
#define FAT32_ATTR_DIRECTORY 0x10
#define FAT32_ATTR_ARCHIVE   0x20
#define FAT32_ATTR_LFN       0x0F

#ifdef __cplusplus
extern "C" {
#endif

/**
 * FAT32 초기화 및 마운트
 */
void FAT32_Init();

#ifdef __cplusplus
}
#endif

#endif
