#ifndef __LIB_STDIO_H
#define __LIB_STDIO_H
#include "stdint.h"
#define va_start(ap, v) ap = (va_list) & v  // 将指针ap指向参数列表开头
#define va_next(ap, t) *((t*)(ap += 4)) //将ap指向下一个参数并转化其类型为t
#define va_end(ap) ap = NULL 
typedef char* va_list;//参数列表
uint32_t printf(const char* format, ...);
uint32_t vsprintf(char* str, const char* format, va_list ap);
uint32_t sprintf(char* buf, const char* format, ...);
#endif