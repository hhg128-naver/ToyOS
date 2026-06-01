#ifndef PIC_H
#define PIC_H

#include "kernel.h"

void PIC_Init();
void PIC_MaskIRQ(uint8_t irq);
void PIC_UnmaskIRQ(uint8_t irq);

#endif
