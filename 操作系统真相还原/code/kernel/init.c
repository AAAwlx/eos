#include "init.h"
#include "interrupt.h"
void init_all()
{
    put_str("initing\n");
    idt_init();//初始化中断门描述符，安装中断处理
    init_time();
    mem_init();
}