#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "kernel.h"
#include <stdint.h>

#define KEYBOARD_BUFFER_SIZE 256

void Keyboard_Handler();
char Keyboard_GetChar();

#endif
