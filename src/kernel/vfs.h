#ifndef VFS_H
#define VFS_H

#include <stdint.h>
#include <stddef.h>

#define VFS_NAME_MAX 256

/**
 * VFS 노드 플래그
 */
#define VFS_FILE        0x01
#define VFS_DIRECTORY   0x02
#define VFS_MOUNTPOINT  0x04

struct VFS_Node;

/**
 * VFS 연산 함수 포인터 정의
 */
typedef uint32_t (*VFS_ReadFunc)(struct VFS_Node* node, uint32_t offset, uint32_t size, uint8_t* buffer);
typedef struct VFS_Node* (*VFS_ReadDirFunc)(struct VFS_Node* node, uint32_t index);
typedef struct VFS_Node* (*VFS_FindDirFunc)(struct VFS_Node* node, char* name);

/**
 * VFS 노드 구조체 (Inode 추상화)
 */
typedef struct VFS_Node {
    char name[VFS_NAME_MAX];
    uint32_t flags;
    uint32_t size;
    uint32_t inode;
    
    // 파일 시스템 전용 데이터
    void* private_data;
    
    VFS_ReadFunc read;
    VFS_ReadDirFunc readdir;
    VFS_FindDirFunc finddir;
    
    struct VFS_Node* ptr; // 마운트 지점 등의 참조용
} VFS_Node;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 시스템 루트 노드
 */
extern VFS_Node* vfs_root;

/**
 * VFS 통합 인터페이스
 */
uint32_t VFS_Read(VFS_Node* node, uint32_t offset, uint32_t size, uint8_t* buffer);
VFS_Node* VFS_ReadDir(VFS_Node* node, uint32_t index);
VFS_Node* VFS_FindDir(VFS_Node* node, char* name);

#ifdef __cplusplus
}
#endif

#endif
