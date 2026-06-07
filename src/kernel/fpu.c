#include <stdio.h>
#include "fpu.h"
#include "kernel.h"
#include "console.h"

void InitializeFPU(void)
{
    uint64_t initial_cr0 = ReadCR0();
    uint64_t initial_cr4 = ReadCR4();
    printf("Initial Processor State - CR0: %p, CR4: %p\n", (void *)initial_cr0, (void *)initial_cr4);

    uint64_t cr0 = initial_cr0;
    cr0 &= ~(1 << 2);
    cr0 |= (1 << 1);
    WriteCR0(cr0);

    uint64_t cr4 = initial_cr4;
    cr4 |= (1 << 9);
    cr4 |= (1 << 10);
    WriteCR4(cr4);

    InitFPU();
    kPrintf("FPU and SSE Initialized.\n");
}
