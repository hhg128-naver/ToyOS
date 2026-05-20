#ifndef LIBUSER_H
#define LIBUSER_H

#include <stdint.h>

void print(const char* str);
void exit(int code);
void sleep(int ms);
void wait(uint64_t pid);

#endif
