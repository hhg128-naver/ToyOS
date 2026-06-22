#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "kernel.h"
#include <stdint.h>

#define KEYBOARD_BUFFER_SIZE 256

#ifdef __cplusplus
extern "C" {
#endif

void Keyboard_Handler();
char Keyboard_GetChar();

#ifdef __cplusplus
}
#endif

#endif
