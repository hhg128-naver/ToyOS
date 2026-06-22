#include "vfs.h"
#include <string.h>

/**
 * 전역 루트 노드
 */
VFS_Node* vfs_root = NULL;

/**
 * 파일 데이터 읽기 추상화 레이어
 */
uint32_t VFS_Read(VFS_Node* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    if (node && node->read) {
        return node->read(node, offset, size, buffer);
    }
    return 0;
}

/**
 * 디렉터리 목록 읽기 추상화 레이어
 */
VFS_Node* VFS_ReadDir(VFS_Node* node, uint32_t index) {
    if (node && (node->flags & VFS_DIRECTORY) && node->readdir) {
        return node->readdir(node, index);
    }
    return NULL;
}

/**
 * 디렉터리 내 파일 탐색 추상화 레이어
 */
VFS_Node* VFS_FindDir(VFS_Node* node, char* name) {
    if (node && (node->flags & VFS_DIRECTORY) && node->finddir) {
        return node->finddir(node, name);
    }
    return NULL;
}
