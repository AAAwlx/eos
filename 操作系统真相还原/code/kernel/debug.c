#include"print.h"
#include"debug.h"
void panic_spin(char* file, int line, char* func, char* condition)
{
    intr_disable();	// 因为有时候会单独调用panic_spin,所以在此处关中断。
    put_str("\n\n\n!!!!! error !!!!!\n");
    put_str("filename:");put_str(file);put_str("\n");
    put_str("line:0x");put_int(line);put_str("\n");
    put_str("function:");put_str((char*)func);put_str("\n");
    put_str("condition:");put_str((char*)condition);put_str("\n");
    while(1);
} 