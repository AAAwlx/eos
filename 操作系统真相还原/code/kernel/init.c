#include "init.h"
#include "interrupt.h"
#include "keyboard.h"
#include "memory.h"
#include "console.h"
#include "syscall-init.h"
#include "thread.h"
#include "timer.h"
#include "tss.h"
#include "ide.h"
void init_all()
{
    put_str("initing\n");
    idt_init();//初始化中断门描述符，安装中断处理
    init_time();
    mem_init();
    thread_init(); // 初始化线程相关结构
    keyboard_init();
    console_init(); 
    tss_init(); 
    //syscall_init();
    //ide_init();  
}