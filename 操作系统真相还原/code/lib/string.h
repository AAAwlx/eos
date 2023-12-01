#ifndef _STRING_
#define _STRING_
#include"stdint.h"
void memset(void* dst_, uint8_t value, uint32_t size);//初始化
void memcpy(void* dst_, const void* src_, uint32_t size);//将后
int memcmp(const void* a_, const void* b_, uint32_t size);//比较字符串
char* strcpy(char* dst_, const char* src_);
uint32_t strlen(const char* str);
int8_t strcmp (const char *a, const char *b); 
char* strchr(const char* string, const uint8_t ch);
char* strrchr(const char* string, const uint8_t ch);
char* strcat(char* dst_, const char* src_);
uint32_t strchrs(const char* filename, uint8_t ch);
#endif