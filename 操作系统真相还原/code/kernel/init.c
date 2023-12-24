#include "init.h"
#include "interrupt.h"
void init_all()
{
    put_str("initing\n");
    idt_init();//初始化中断门描述符，安装中断处理
    init_time();
    mem_init();
    thread_init(); // 初始化线程相关结构
    console_init(); 
    keyboard_init();
}