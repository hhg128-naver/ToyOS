#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "shell.h"
#include "kernel.h"
#include "pmm.h"

#define MAX_COMMAND_LEN 128

static void command_help();
static void command_clear();
static void command_mem();

void Shell_Main() {
    char input[MAX_COMMAND_LEN];

    printf("\n--- ToyOS Interactive Shell ---\n");
    printf("Type 'help' for a list of commands.\n\n");

    while (1) {
        printf("ToyOS> ");
        
        /* 표준 입력을 통해 한 줄 읽기 */
        if (fgets(input, MAX_COMMAND_LEN, stdin) == NULL) {
            continue;
        }

        /* 개행 문자 제거 */
        input[strcspn(input, "\n")] = 0;

        /* 명령어 처리 */
        if (strlen(input) == 0) {
            continue;
        }

        if (strcmp(input, "help") == 0) {
            command_help();
        } else if (strcmp(input, "clear") == 0) {
            command_clear();
        } else if (strcmp(input, "mem") == 0) {
            command_mem();
        } else if (strcmp(input, "reboot") == 0) {
            printf("Rebooting is not implemented yet.\n");
        } else {
            printf("Unknown command: '%s'\n", input);
        }
    }
}

static void command_help() {
    printf("Available commands:\n");
    printf("  help   - Show this help message\n");
    printf("  clear  - Clear the screen\n");
    printf("  mem    - Show physical memory status\n");
    printf("  reboot - Restart the system\n");
}

static void command_clear() {
    extern BootInfo *boot_info_global;
    /* kernel.c에 정의된 ClearScreen 호출 */
    ClearScreen(boot_info_global, 0x00000033);
}

static void command_mem() {
    uint64_t total = PMM_GetTotalMemory();
    uint64_t free = PMM_GetFreeMemory();
    uint64_t used = total - free;

    printf("Physical Memory Status:\n");
    printf("  Total: %d MB\n", (int)(total / (1024 * 1024)));
    printf("  Used : %d MB\n", (int)(used / (1024 * 1024)));
    printf("  Free : %d MB\n", (int)(free / (1024 * 1024)));
}
