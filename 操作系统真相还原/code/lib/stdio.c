# include "stdio.h"
# include "syscall.h"
# include "string.h"
#include "stdint.h"
#include "global.h"
#define va_start(ap, v) ap = (va_list) & v  // 将指针ap指向参数列表开头
#define va_next(ap, t) *((t*)(ap += 4));//将ap指向下一个参数并转化其类型为t
# define va_end(ap) ap = NULL 
static void itoa(uint32_t value, char** buf_ptr_addr, uint8_t base)
{
    uint32_t m = value % base;//求模拆出最低位
    uint32_t i = value / base;//取整
    if(i){
        itoa(i, buf_ptr_addr, base);//如果待转化的数不为0就继续转化
    }
    if (m<10)
    {
        *((*buf_ptr_addr)++) = m + '0';
    } else {
        *((*buf_ptr_addr)++) = m + 'A' - 10;
    }
}
uint32_t vsprintf(char* str, const char* format, va_list ap)
{
    char* str_buffer = str;
    const char* index_ptr = format;
    char index_char = *index_ptr;
    char* arg_str;//参数中%s代表的字符串的占位符
    int32_t arg_int;//整数的占位符
    while (index_char)
    {
        if (index_char!='%')
        {
            *(str_buffer++) = index_char;
            index_char = *(++index_ptr);
            continue;
        }
        index_char=*(++index_ptr);
        switch (index_char)
        {
            case 'c':
                *(str_buffer++) = va_next(ap,char);
                index_char = *(++index_ptr);
                break;
            case 's':
                arg_str = va_next(ap, char*);
                strcpy(str_buffer, arg_str);
                str_buffer += strlen(arg_str);
                index_char = *(++index_ptr);
                break;
            case 'x':
                arg_int = va_next(ap, int);
                itoa(arg_int, &str_buffer, 16);
                index_char = *(++index_ptr);
                break;
            case 'd':
                arg_int = va_next(ap, int);
                if (arg_int<0)//如果小于0,就先输出-号再转化
                {
                    arg_int = 0 - arg_int;
                    *(str_buffer++) = '-';
                }
                itoa(arg_int, &str_buffer, 16);
                index_char = *(++index_ptr);
                break;
        }
    }
    return strlen(str);
}
uint32_t printf(const char* format, ...) 
{
    va_list args;
    uint32_t retval;
    va_start(args, format);
    char buffer[1024] = {0};
    vsprintf(buffer, format, args);
    va_end(args);

}