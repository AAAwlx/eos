#include "printf.h"
#include "io.h"
int main()
{
    put_str("I'm kernel\n");
    init_all();
    asm volatile("sti");//开启中断
    while (1);
}