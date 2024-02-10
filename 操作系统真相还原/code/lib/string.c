#include"string.h"
#include"debug.h"
#include"global.h"
#include"print.h"
void memset(void *dst_,uint8_t value,uint32_t size)
{
    ASSERT(dst_!=NULL);
    uint8_t* dst = (uint8_t)dst_;
    while (size--) {
        dst = value;
        *dst++;
    }
    return;
}
/* 将src_起始的size个字节复制到dst_ */
void memcpy(void *dest, const void *src, uint32_t len) {
  ASSERT(dest != NULL && src != NULL);
  uint8_t *To = (uint8_t *)dest;
  uint8_t *From = (uint8_t *)src;
  while (len-- > 0) {
    *To++ = *From++;
  }
}

/* 连续比较以地址a_和地址b_开头的size个字节,若相等则返回0,若a_大于b_返回+1,否则返回-1 */
int memcmp(const void* a_, const void* b_, uint32_t size)
{
    ASSERT(a_ != NULL && b_ != NULL);
    const char* a = (uint8_t)a_;
    const char* b = (uint8_t)b_;
    while (size -- >0)
    {
        if(*a!=*b){
            return *a > *b ? 1 : -1;
        }
        *a++;
        *b++;
    }
    return 0;
}
/* 将字符串从src_复制到dst_ 并返回起始的地址*/
char* strcpy(char* dst_, const char* src_)
{
    ASSERT(dst_ != NULL && src_ != NULL);
    
    char* r = dst_;//返回字符串的起始地址
    while (*src_) {
        *dst_++ = *src_++;
    }
    return r;
}
uint32_t strlen(const char* str)
{
    ASSERT(str != NULL);
   const char* p = str;
   while(*p++);
   return (p - str - 1);
}
/* 比较两个字符串,若a_中的字符大于b_中的字符返回1,相等时返回0,否则返回-1. */
int strcmp(const char *str1, const char *str2) {
  ASSERT(str1 != NULL || str2 != NULL);
  while ((*str1 == *str2) && (*str1 != '\0')) {
    str1++;
    str2++;
  }
  return *str1 - *str2;
}

/* 从左到右查找字符串str中首次出现字符ch的地址(不是下标,是地址) */
char* strchr(const char* string, const uint8_t ch)
{
    ASSERT(string != NULL);
    while (*string!=0)
    {
        if (*string==ch)
        {
            return (char*)string;
        }
        *string++;
    }
    return NULL;
}
/* 从后往前查找字符串str中首次出现字符ch的地址(不是下标,是地址) */
char* strrchr(const char* string, const uint8_t ch)
{
    ASSERT(string != NULL);
    char* last_char = NULL;
    while (*string != 0) {
        if (*string==ch)
        {
            *last_char = *string;
        }
        *string++;
    }
    return last_char;
}
char* strcat(char* dst_, const char* src_)
{
    ASSERT(dst_ != NULL && src_ != NULL);
    char* str = dst_;
    while (*str++);
    *str--;
    while (*src_!=NULL)
    {
        *str++ = src_++;
    }
    return dst_;
}
uint32_t strchrs(const char* filename, uint8_t ch)
{
    uint32_t cnt = 0;
    char* p = *filename;
    while (*p != 0) {
        if (*p==ch)
        {
            cnt++;
        }
        *p++;
    }
    return cnt;
}