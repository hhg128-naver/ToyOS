#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include "kernel.h"
#include "vmm.h"
#include "pmm.h"
#include "graphics.h"

/* Newlib 전용 힙 영역 설정 */
#define NEWLIB_HEAP_START 0x02000000
#define NEWLIB_HEAP_SIZE  (2 * 1024 * 1024)

static char* heap_end = NULL;

int errno;
int* __errno() { return &errno; }

extern "C" {

void* sbrk(ptrdiff_t incr)
{
	char logStr[100];
	sprintf(logStr, "sbrk called with incr: %d\n", (int)incr);
	kPrintf(logStr);

	char* prev_heap_end;
	if (heap_end == NULL)
	{
		heap_end = (char*)NEWLIB_HEAP_START;
		for (uint64_t addr = NEWLIB_HEAP_START; addr < NEWLIB_HEAP_START + NEWLIB_HEAP_SIZE; addr += PAGE_SIZE)
		{
			void* phys = PMM_AllocPage();
			if (phys == NULL)
			{
				errno = ENOMEM;
				return (void*)-1;
			}
			VMM_MapPage((void*)addr, phys, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
		}
	}
	prev_heap_end = heap_end;
	if (heap_end + incr > (char*)(NEWLIB_HEAP_START + NEWLIB_HEAP_SIZE))
	{
		errno = ENOMEM;
		return (void*)-1;
	}
	heap_end += incr;
	return (void*)prev_heap_end;
}

/*
 * write: 데이터 출력을 위한 시스템 콜. printf() 등에서 호출됩니다.
 */
int write(int file, char* ptr, int len)
{
	if (file == 1 || file == 2)
	{ // stdout 또는 stderr
		if (g_ShellLayer != NULL)
		{
			for (int i = 0; i < len; i++)
			{
				Layer_TerminalPutChar(g_ShellLayer, ptr[i], 0x00FFFFFF, 0x00C6C6C6);
			}
		}
		else
		{
			for (int i = 0; i < len; i++)
			{
				char buf[2] = { ptr[i], '\0' };
				kPrintf(buf);
			}
		}
		return len;
	}
	errno = EBADF;
	return -1;
}

#include "keyboard.h"

int read(int file, char* ptr, int len)
{
	if (file == 0)
	{ // stdin
		int i = 0;
		while (i < len)
		{
			char c = Keyboard_GetChar();
			if (c == '\b')
			{
				if (i > 0)
				{
					i--;
					if (g_ShellLayer)
					{
						Layer_TerminalPutChar(g_ShellLayer, '\b', 0x00FFFFFF, 0x00C6C6C6);
					}
				}
				continue;
			}
			ptr[i] = c;
			if (g_ShellLayer)
			{
				Layer_TerminalPutChar(g_ShellLayer, c, 0x00FFFFFF, 0x00C6C6C6);
			}

			if (ptr[i] == '\n')
			{
				i++;
				break;
			}
			i++;
		}
		return i;
	}
	errno = EBADF;
	return -1;
}

int close(int file) { return -1; }
int fstat(int file, struct stat* st)
{
	st->st_mode = S_IFCHR;
	return 0;
}
int isatty(int file) { return 1; }
int lseek(int file, int ptr, int dir) { return 0; }
void _exit(int status)
{
	__asm__ volatile (
		"mov $2, %%rax\n"
		"syscall"
		: : : "rax", "rcx", "r11", "memory"
		);
	while (1);
}
int kill(int n, int m)
{
	errno = EINVAL;
	return -1;
}
int getpid() { return 1; }

void* _sbrk(ptrdiff_t incr) { return sbrk(incr); }
int _write(int file, char* ptr, int len) { return write(file, ptr, len); }
int _read(int file, char* ptr, int len) { return read(file, ptr, len); }
int _close(int file) { return close(file); }
int _fstat(int file, struct stat* st) { return fstat(file, st); }
int _isatty(int file) { return isatty(file); }
int _lseek(int file, int ptr, int dir) { return lseek(file, ptr, dir); }
int _kill(int n, int m) { return kill(n, m); }
int _getpid() { return getpid(); }

}
