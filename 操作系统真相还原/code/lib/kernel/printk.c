#include"global.h"
#include"stdio.h"
uint32_t printk(const char* format, ...) 
{
    va_list args;
    uint32_t retval;
    va_start(args, format);
    char buffer[1024] = {0};
    vsprintf(buffer, format, args);
    va_end(args);
    return console_put_str(buffer);
}