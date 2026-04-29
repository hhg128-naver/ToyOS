#include "fat32.h"
#include "ide.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * FAT32 전역 상태 관리 변수
 */
static FAT32_BPB bpb;
static uint32_t partition_lba_start = 0;
static uint32_t first_data_sector;
static uint32_t sectors_per_cluster;

/**
 * MBR 파티션 엔트리 구조체
 */
typedef struct {
    uint8_t  boot_indicator;
    uint8_t  starting_chs[3];
    uint8_t  os_type;
    uint8_t  ending_chs[3];
    uint32_t starting_lba;
    uint32_t size_in_sectors;
} __attribute__((packed)) MBR_PartitionEntry;

/**
 * MBR 섹터 구조체
 */
typedef struct {
    uint8_t  boot_code[446];
    MBR_PartitionEntry partitions[4];
    uint16_t signature;
} __attribute__((packed)) MBR_Sector;

// VFS 연산 구현부 전방 선언
static uint32_t FAT32_Read(VFS_Node* node, uint32_t offset, uint32_t size, uint8_t* buffer);
static VFS_Node* FAT32_ReadDir(VFS_Node* node, uint32_t index);

/**
 * 클러스터 번호로부터 디스크 섹터 번호 계산
 */
static uint32_t FAT32_GetSectorFromCluster(uint32_t cluster) {
    return partition_lba_start + first_data_sector + ((cluster - 2) * sectors_per_cluster);
}

/**
 * FAT 테이블을 참조하여 다음 클러스터 번호 획득
 */
static uint32_t FAT32_GetNextCluster(uint32_t cluster) {
    // 한 섹터에 512 / 4 = 128개의 FAT 엔트리가 들어감
    uint32_t fat_sector = partition_lba_start + bpb.reserved_sectors + (cluster * 4 / bpb.bytes_per_sector);
    uint32_t fat_offset = (cluster * 4) % bpb.bytes_per_sector;
    
    uint8_t buffer[512];
    IDE_ReadSectors(fat_sector, 1, buffer);
    
    uint32_t next_cluster = *(uint32_t*)(&buffer[fat_offset]);
    return next_cluster & 0x0FFFFFFF; // 상위 4비트는 예약됨
}

/**
 * FAT32 초기화 및 루트 디렉터리 마운트
 */
void FAT32_Init() {
    uint8_t boot_sector[512];
    partition_lba_start = 0;

    // 0번 섹터 읽기
    IDE_ReadSectors(0, 1, boot_sector);
    
    // 1. 0번 섹터가 이미 FAT 부트 섹터인지 확인 (Superfloppy)
    // FAT16은 offset 54, FAT32는 offset 82에 시그니처가 있음
    if (strncmp((char*)&boot_sector[54], "FAT", 3) == 0 || 
        strncmp((char*)&boot_sector[82], "FAT", 3) == 0) {
        partition_lba_start = 0;
        printf("FAT: Superfloppy (no partition) detected.\n");
    } 
    // 2. MBR 파티션 테이블 확인
    else {
        MBR_Sector* mbr = (MBR_Sector*)boot_sector;
        if (mbr->signature == 0xAA55) {
            // 첫 번째 파티션 정보 확인 (FAT12: 0x01, FAT16: 0x04/0x06/0x0E, FAT32: 0x0B/0x0C)
            uint8_t type = mbr->partitions[0].os_type;
            if (type == 0x01 || type == 0x04 || type == 0x06 || type == 0x0E || type == 0x0B || type == 0x0C) {
                partition_lba_start = mbr->partitions[0].starting_lba;
                printf("FAT: Partition found at LBA %d (Type: 0x%02x)\n", 
                       partition_lba_start, type);
                IDE_ReadSectors(partition_lba_start, 1, boot_sector);
            }
        }
    }

    // BPB 구조체로 복사
    memcpy(&bpb, boot_sector, sizeof(FAT32_BPB));
    
    // 최종 시그니처 확인 (FAT16 또는 FAT32)
    int is_fat32 = (strncmp(bpb.fs_type, "FAT32", 5) == 0);
    // FAT16의 경우 fs_type 위치가 다름 (bpb.fs_type은 offset 82를 가리키므로 FAT16에서는 volume_label 위치)
    int is_fat16 = (strncmp((char*)&boot_sector[54], "FAT16", 5) == 0 || 
                    strncmp((char*)&boot_sector[54], "FAT", 3) == 0);

    if (!is_fat32 && !is_fat16) {
        printf("FAT: Unknown file system type. Signature at 54: '%.8s', at 82: '%.8s'\n", 
               &boot_sector[54], &boot_sector[82]);
        return;
    }
    
    if (is_fat16) {
        printf("FAT: FAT16 detected. Note: This driver is optimized for FAT32.\n");
        // FAT16 호환을 위해 필요한 값들을 보정 (최소한의 읽기 기능)
        // FAT16은 sectors_per_fat_32 대신 sectors_per_fat_16을 사용함
        if (bpb.sectors_per_fat_32 == 0) {
            bpb.sectors_per_fat_32 = bpb.sectors_per_fat_16;
        }
    }

    // 주요 오프셋 계산
    uint32_t fat_size = bpb.sectors_per_fat_32;
    first_data_sector = bpb.reserved_sectors + (bpb.num_fats * fat_size);
    sectors_per_cluster = bpb.sectors_per_cluster;
    
    // 데이터 검증: sectors_per_cluster가 0이거나 너무 크면 위험함 (IDE_ReadSectors에서 256섹터 읽기 발생 가능)
    if (sectors_per_cluster == 0 || (sectors_per_cluster & (sectors_per_cluster - 1)) != 0) {
        printf("FAT: Invalid sectors_per_cluster (%d). Aborting mount.\n", sectors_per_cluster);
        return;
    }
    
    printf("FAT32: Bytes/Sector: %d, Sectors/Cluster: %d\n", bpb.bytes_per_sector, sectors_per_cluster);
    printf("FAT32: Root Cluster: %d, First Data Sector: %d\n", bpb.root_cluster, first_data_sector);
    
    // 루트 VFS 노드 생성
    vfs_root = (VFS_Node*)malloc(sizeof(VFS_Node));
    if (!vfs_root) return;
    
    memset(vfs_root, 0, sizeof(VFS_Node));
    strcpy(vfs_root->name, "/");
    vfs_root->flags = VFS_DIRECTORY;
    vfs_root->size = 0;
    vfs_root->inode = bpb.root_cluster;
    vfs_root->readdir = FAT32_ReadDir;
    vfs_root->read = NULL;
    
    printf("FAT32: Mounted successfully.\n");
}

/**
 * 디렉터리 엔트리 읽기 (index 번째 엔트리 반환)
 */
static VFS_Node* FAT32_ReadDir(VFS_Node* node, uint32_t index) {
    uint32_t cluster = node->inode;
    uint32_t cluster_size = sectors_per_cluster * 512;
    uint8_t* buffer = (uint8_t*)malloc(cluster_size);
    if (!buffer) return NULL;
    
    uint32_t current_index = 0;
    
    // 클러스터 체인을 따라가며 디렉터리 탐색
    while (cluster < 0x0FFFFFF8) {
        uint32_t sector = FAT32_GetSectorFromCluster(cluster);
        IDE_ReadSectors(sector, sectors_per_cluster, buffer);
        
        FAT32_DirEntry* entries = (FAT32_DirEntry*)buffer;
        for (uint32_t i = 0; i < (cluster_size / 32); i++) {
            // 엔트리 끝 확인
            if (entries[i].name[0] == 0x00) {
                free(buffer);
                return NULL;
            }
            // 삭제된 엔트리 또는 LFN(Long File Name) 건너뜀
            if ((uint8_t)entries[i].name[0] == 0xE5) continue;
            if (entries[i].attr == FAT32_ATTR_LFN) continue;
            // 볼륨 라벨 건너뜀
            if (entries[i].attr & FAT32_ATTR_VOLUME_ID) continue;
            
            if (current_index == index) {
                VFS_Node* child = (VFS_Node*)malloc(sizeof(VFS_Node));
                if (!child) {
                    free(buffer);
                    return NULL;
                }
                memset(child, 0, sizeof(VFS_Node));
                
                // 8.3 형식을 읽기 좋은 문자열로 변환 (예: "KERNEL  BIN" -> "KERNEL.BIN")
                int pos = 0;
                for (int j = 0; j < 8 && entries[i].name[j] != ' '; j++) child->name[pos++] = entries[i].name[j];
                
                if (!(entries[i].attr & FAT32_ATTR_DIRECTORY)) {
                    child->name[pos++] = '.';
                    for (int j = 0; j < 3 && entries[i].ext[j] != ' '; j++) child->name[pos++] = entries[i].ext[j];
                }
                child->name[pos] = '\0';
                
                child->flags = (entries[i].attr & FAT32_ATTR_DIRECTORY) ? VFS_DIRECTORY : VFS_FILE;
                child->size = entries[i].size;
                child->inode = (entries[i].cluster_high << 16) | entries[i].cluster_low;
                child->read = FAT32_Read;
                child->readdir = FAT32_ReadDir;
                
                free(buffer);
                return child;
            }
            current_index++;
        }
        cluster = FAT32_GetNextCluster(cluster);
    }
    
    free(buffer);
    return NULL;
}

/**
 * 파일 데이터 읽기
 */
static uint32_t FAT32_Read(VFS_Node* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    if (offset >= node->size) return 0;
    if (offset + size > node->size) size = node->size - offset;
    
    uint32_t cluster = node->inode;
    uint32_t cluster_size = sectors_per_cluster * 512;
    
    // 시작 오프셋이 있는 클러스터까지 건너뜀
    uint32_t clusters_to_skip = offset / cluster_size;
    for (uint32_t i = 0; i < clusters_to_skip; i++) {
        cluster = FAT32_GetNextCluster(cluster);
        if (cluster >= 0x0FFFFFF8) return 0;
    }
    
    uint32_t bytes_read = 0;
    uint32_t cluster_offset = offset % cluster_size;
    
    uint8_t* temp_buffer = (uint8_t*)malloc(cluster_size);
    if (!temp_buffer) return 0;
    
    while (bytes_read < size) {
        uint32_t sector = FAT32_GetSectorFromCluster(cluster);
        IDE_ReadSectors(sector, sectors_per_cluster, temp_buffer);
        
        uint32_t to_copy = cluster_size - cluster_offset;
        if (bytes_read + to_copy > size) to_copy = size - bytes_read;
        
        memcpy(buffer + bytes_read, temp_buffer + cluster_offset, to_copy);
        
        bytes_read += to_copy;
        cluster_offset = 0; // 첫 클러스터 이후부터는 항상 오프셋 0부터 시작
        
        if (bytes_read < size) {
            cluster = FAT32_GetNextCluster(cluster);
            if (cluster >= 0x0FFFFFF8) break;
        }
    }
    
    free(temp_buffer);
    return bytes_read;
}
