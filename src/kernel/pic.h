#ifndef PIC_H
#define PIC_H

#include "kernel.h"

#define PIC_IRQ_PIT            0  /* Programmable Interval Timer */
#define PIC_IRQ_KEYBOARD       1  /* Keyboard Controller */
#define PIC_IRQ_SLAVE          2  /* Cascade for PIC2 */
#define PIC_IRQ_SERIAL2        3  /* COM2 */
#define PIC_IRQ_SERIAL1        4  /* COM1 */
#define PIC_IRQ_LPT2           5  /* LPT2 */
#define PIC_IRQ_FLOPPY         6  /* Floppy Controller */
#define PIC_IRQ_LPT1           7  /* LPT1 */
#define PIC_IRQ_RTC            8  /* Real-Time Clock */
#define PIC_IRQ_ACPI           9  /* ACPI */
#define PIC_IRQ_SCSI1          10 /* SCSI/NIC */
#define PIC_IRQ_SCSI2          11 /* SCSI/NIC */
#define PIC_IRQ_PS2MOUSE       12 /* PS/2 Mouse */
#define PIC_IRQ_FPU            13 /* FPU/Coprocessor */
#define PIC_IRQ_PRIMARY_ATA    14 /* Primary IDE */
#define PIC_IRQ_SECONDARY_ATA  15 /* Secondary IDE */

#ifdef __cplusplus
extern "C" {
#endif

void PIC_Init();
void PIC_MaskIRQ(uint8_t irq);
void PIC_UnmaskIRQ(uint8_t irq);
void PIC_Disable(void);

#ifdef __cplusplus
}
#endif

#endif
