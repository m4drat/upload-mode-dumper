#ifndef HEXDUMP_H
#define HEXDUMP_H

#define _CRT_SECURE_NO_WARNINGS

#include <stdint.h>

#define HEXDUMP_COLS (16)

void hexdump(void* mem, uint32_t len, uint64_t base);

#endif // HEXDUMP_H