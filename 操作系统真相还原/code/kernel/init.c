#include "init.h"
#include "interrupt.h"
void init_all()
{
    put_str("initing\n");
    init_idt();//初始化中断门描述符，安装中断处理
    init_time();
}