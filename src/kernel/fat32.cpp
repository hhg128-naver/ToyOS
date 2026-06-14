#include "fat32.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "console.h"

extern "C" {
    #include "ide.h"
    #include "vfs.h"
}

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

// C++ FAT32 파일 시스템 클래스 정의
class FAT32FileSystem {
private:
    FAT32_BPB bpb;
    uint32_t partition_lba_start;
    uint32_t first_data_sector;
    uint32_t sectors_per_cluster;

public:
    FAT32FileSystem() : partition_lba_start(0), first_data_sector(0), sectors_per_cluster(0) {
        memset(&bpb, 0, sizeof(FAT32_BPB));
    }

    uint32_t getSectorFromCluster(uint32_t cluster) const {
        return partition_lba_start + first_data_sector + ((cluster - 2) * sectors_per_cluster);
    }

    uint32_t getNextCluster(uint32_t cluster) const {
        uint32_t fat_sector = partition_lba_start + bpb.reserved_sectors + (cluster * 4 / bpb.bytes_per_sector);
        uint32_t fat_offset = (cluster * 4) % bpb.bytes_per_sector;
        
        uint8_t buffer[512];
        IDE_ReadSectors(fat_sector, 1, buffer);
        
        uint32_t next_cluster = *(uint32_t*)(&buffer[fat_offset]);
        return next_cluster & 0x0FFFFFFF; // 상위 4비트는 예약됨
    }

    void init();
    VFS_Node* readDir(VFS_Node* node, uint32_t index);
    uint32_t read(VFS_Node* node, uint32_t offset, uint32_t size, uint8_t* buffer);
};

// 전역 FAT32 인스턴스
FAT32FileSystem g_FAT32;

// VFS 연동용 브릿지 함수 전방 선언 (C 스타일)
extern "C" {
    static uint32_t FAT32_ReadBridge(VFS_Node* node, uint32_t offset, uint32_t size, uint8_t* buffer);
    static VFS_Node* FAT32_ReadDirBridge(VFS_Node* node, uint32_t index);
}

void FAT32FileSystem::init() {
    uint8_t boot_sector[512];
    partition_lba_start = 0;

    // 0번 섹터 읽기
    IDE_ReadSectors(0, 1, boot_sector);
    
    // 1. 0번 섹터가 이미 FAT 부트 섹터인지 확인 (Superfloppy)
    if (strncmp((char*)&boot_sector[54], "FAT", 3) == 0 || 
        strncmp((char*)&boot_sector[82], "FAT", 3) == 0) {
        partition_lba_start = 0;
        kPrintf("FAT: Superfloppy (no partition) detected.\n");
    } 
    // 2. MBR 파티션 테이블 확인
    else {
        MBR_Sector* mbr = (MBR_Sector*)boot_sector;
        if (mbr->signature == 0xAA55) {
            uint8_t type = mbr->partitions[0].os_type;
            if (type == 0x01 || type == 0x04 || type == 0x06 || type == 0x0E || type == 0x0B || type == 0x0C) {
                partition_lba_start = mbr->partitions[0].starting_lba;
                kPrintf("FAT: Partition found at LBA %d (Type: 0x%02x)\n", 
                       partition_lba_start, type);
                IDE_ReadSectors(partition_lba_start, 1, boot_sector);
            }
        }
    }

    // BPB 구조체로 복사
    memcpy(&bpb, boot_sector, sizeof(FAT32_BPB));
    
    // 최종 시그니처 확인 (FAT16 또는 FAT32)
    int is_fat32 = (strncmp(bpb.fs_type, "FAT32", 5) == 0);
    int is_fat16 = (strncmp((char*)&boot_sector[54], "FAT16", 5) == 0 || 
                    strncmp((char*)&boot_sector[54], "FAT", 3) == 0);

    if (!is_fat32 && !is_fat16) {
        kPrintf("FAT: Unknown file system type. Signature at 54: '%.8s', at 82: '%.8s'\n", 
               &boot_sector[54], &boot_sector[82]);
        return;
    }
    
    if (is_fat16) {
        kPrintf("FAT: FAT16 detected. Note: This driver is optimized for FAT32.\n");
        if (bpb.sectors_per_fat_32 == 0) {
            bpb.sectors_per_fat_32 = bpb.sectors_per_fat_16;
        }
    }

    // 주요 오프셋 계산
    uint32_t fat_size = bpb.sectors_per_fat_32;
    first_data_sector = bpb.reserved_sectors + (bpb.num_fats * fat_size);
    sectors_per_cluster = bpb.sectors_per_cluster;
    
    // 데이터 검증: sectors_per_cluster가 0이거나 너무 크면 위험함
    if (sectors_per_cluster == 0 || (sectors_per_cluster & (sectors_per_cluster - 1)) != 0) {
        kPrintf("FAT: Invalid sectors_per_cluster (%d). Aborting mount.\n", sectors_per_cluster);
        return;
    }
    
    kPrintf("FAT32: Bytes/Sector: %d, Sectors/Cluster: %d\n", bpb.bytes_per_sector, sectors_per_cluster);
    kPrintf("FAT32: Root Cluster: %d, First Data Sector: %d\n", bpb.root_cluster, first_data_sector);
    
    // 루트 VFS 노드 생성 (C++ new 대신 기존 c와 매칭을 위해 malloc 사용)
    vfs_root = (VFS_Node*)malloc(sizeof(VFS_Node));
    if (!vfs_root) return;
    
    memset(vfs_root, 0, sizeof(VFS_Node));
    strcpy(vfs_root->name, "/");
    vfs_root->flags = VFS_DIRECTORY;
    vfs_root->size = 0;
    vfs_root->inode = bpb.root_cluster;
    vfs_root->readdir = FAT32_ReadDirBridge;
    vfs_root->read = NULL;
    
    kPrintf("FAT32: Mounted successfully.\n");
}

VFS_Node* FAT32FileSystem::readDir(VFS_Node* node, uint32_t index) {
    uint32_t cluster = node->inode;
    uint32_t cluster_size = sectors_per_cluster * 512;
    uint8_t* buffer = (uint8_t*)malloc(cluster_size);
    if (!buffer) return NULL;
    
    uint32_t current_index = 0;
    
    // 클러스터 체인을 따라가며 디렉터리 탐색
    while (cluster < 0x0FFFFFF8) {
        uint32_t sector = getSectorFromCluster(cluster);
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
                
                // 8.3 형식을 읽기 좋은 문자열(소문자)로 변환
                int pos = 0;
                for (int j = 0; j < 8 && entries[i].name[j] != ' '; j++) {
                    char c = entries[i].name[j];
                    if (c >= 'A' && c <= 'Z') c += 32; // 소문자 변환
                    child->name[pos++] = c;
                }
                
                if (!(entries[i].attr & FAT32_ATTR_DIRECTORY)) {
                    child->name[pos++] = '.';
                    for (int j = 0; j < 3 && entries[i].ext[j] != ' '; j++) {
                        char c = entries[i].ext[j];
                        if (c >= 'A' && c <= 'Z') c += 32; // 소문자 변환
                        child->name[pos++] = c;
                    }
                }
                child->name[pos] = '\0';
                
                child->flags = (entries[i].attr & FAT32_ATTR_DIRECTORY) ? VFS_DIRECTORY : VFS_FILE;
                child->size = entries[i].size;
                child->inode = (entries[i].cluster_high << 16) | entries[i].cluster_low;
                child->read = FAT32_ReadBridge;
                child->readdir = FAT32_ReadDirBridge;
                
                free(buffer);
                return child;
            }
            current_index++;
        }
        cluster = getNextCluster(cluster);
    }
    
    free(buffer);
    return NULL;
}

uint32_t FAT32FileSystem::read(VFS_Node* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    if (offset >= node->size) return 0;
    if (offset + size > node->size) size = node->size - offset;
    
    uint32_t cluster = node->inode;
    uint32_t cluster_size = sectors_per_cluster * 512;
    
    // 시작 오프셋이 있는 클러스터까지 건너뜀
    uint32_t clusters_to_skip = offset / cluster_size;
    for (uint32_t i = 0; i < clusters_to_skip; i++) {
        cluster = getNextCluster(cluster);
        if (cluster >= 0x0FFFFFF8) return 0;
    }
    
    uint32_t bytes_read = 0;
    uint32_t cluster_offset = offset % cluster_size;
    
    uint8_t* temp_buffer = (uint8_t*)malloc(cluster_size);
    if (!temp_buffer) return 0;
    
    while (bytes_read < size) {
        uint32_t sector = getSectorFromCluster(cluster);
        IDE_ReadSectors(sector, sectors_per_cluster, temp_buffer);
        
        uint32_t to_copy = cluster_size - cluster_offset;
        if (bytes_read + to_copy > size) to_copy = size - bytes_read;
        
        memcpy(buffer + bytes_read, temp_buffer + cluster_offset, to_copy);
        
        bytes_read += to_copy;
        cluster_offset = 0; // 첫 클러스터 이후부터는 항상 오프셋 0부터 시작
        
        if (bytes_read < size) {
            cluster = getNextCluster(cluster);
            if (cluster >= 0x0FFFFFF8) break;
        }
    }
    
    free(temp_buffer);
    return bytes_read;
}

// C 스타일 브릿지 함수 정의
extern "C" {
    void FAT32_Init() {
        g_FAT32.init();
    }

    static uint32_t FAT32_ReadBridge(VFS_Node* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
        return g_FAT32.read(node, offset, size, buffer);
    }

    static VFS_Node* FAT32_ReadDirBridge(VFS_Node* node, uint32_t index) {
        return g_FAT32.readDir(node, index);
    }
}
