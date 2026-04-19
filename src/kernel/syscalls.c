#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include "kernel.h"
#include "vmm.h"
#include "pmm.h"

/* Newlib 전용 힙 영역 설정 */
#define NEWLIB_HEAP_START 0x02000000
#define NEWLIB_HEAP_SIZE  (2 * 1024 * 1024)

static char *heap_end = NULL;

/* 
 * errno 정의: Newlib 표준 라이브러리와의 링크 호환성을 위해 직접 제공합니다.
 */
int errno;
int * __errno() { return &errno; }

/* 
 * sbrk: 메모리 할당을 위한 시스템 콜. malloc()에서 호출됩니다.
 */
void * sbrk(ptrdiff_t incr) {
    char *prev_heap_end;

    if (heap_end == NULL) {
        heap_end = (char *)NEWLIB_HEAP_START;
        
        /* 첫 호출 시 힙 영역 전체를 미리 매핑 (Static 방식) */
        for (uint64_t addr = NEWLIB_HEAP_START; addr < NEWLIB_HEAP_START + NEWLIB_HEAP_SIZE; addr += PAGE_SIZE) {
            void* phys = PMM_AllocPage();
            if (phys == NULL) {
                errno = ENOMEM;
                return (void *)-1;
            }
            VMM_MapPage((void*)addr, phys, PAGE_PRESENT | PAGE_WRITABLE);
        }
    }

    prev_heap_end = heap_end;

    if (heap_end + incr > (char *)(NEWLIB_HEAP_START + NEWLIB_HEAP_SIZE)) {
        errno = ENOMEM;
        return (void *)-1;
    }

    heap_end += incr;
    return (void *)prev_heap_end;
}

/* 
 * write: 데이터 출력을 위한 시스템 콜. printf() 등에서 호출됩니다.
 */
int write(int file, char *ptr, int len) {
    if (file == 1 || file == 2) { // stdout 또는 stderr
        for (int i = 0; i < len; i++) {
             char buf[2] = {ptr[i], '\0'};
             Printf(buf);
        }
        return len;
    }
    errno = EBADF;
    return -1;
}

/* 
 * read: 데이터 입력을 위한 시스템 콜. scanf() 등에서 호출됩니다. 
 */
int read(int file, char *ptr, int len) {
    if (file == 0) { // stdin
        return 0;
    }
    errno = EBADF;
    return -1;
}

/* 
 * 기타 필수 스텁 함수들 (언더바 제거)
 */
int close(int file) { return -1; }
int fstat(int file, struct stat *st) {
    st->st_mode = S_IFCHR;
    return 0;
}
int isatty(int file) { return 1; }
int lseek(int file, int ptr, int dir) { return 0; }
void _exit(int status) {
    while(1);
}
int kill(int n, int m) {
    errno = EINVAL;
    return -1;
}
int getpid() {
    return 1;
}

/* Newlib 내부에서 _로 시작하는 이름을 찾을 경우를 위해 별칭 추가 (필요 시) */
void * _sbrk(ptrdiff_t incr) __attribute__((alias("sbrk")));
int _write(int file, char *ptr, int len) __attribute__((alias("write")));
int _read(int file, char *ptr, int len) __attribute__((alias("read")));
int _close(int file) __attribute__((alias("close")));
int _fstat(int file, struct stat *st) __attribute__((alias("fstat")));
int _isatty(int file) __attribute__((alias("isatty")));
int _lseek(int file, int ptr, int dir) __attribute__((alias("lseek")));
int _kill(int n, int m) __attribute__((alias("kill")));
int _getpid() __attribute__((alias("getpid")));
