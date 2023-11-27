#ifndef LIB_KERNEL_IO
#define LIB_KERNEL_IO

#include "stdint.h"

void static inline void outb(uint16_t port, uint8_t value);
void outw(uint16_t port, const void* addr, uint32_t size);
static inline uint8_t inb(uint16_t port);
void inw(uint16_t port, const void* addr, uint32_t size);
#endif