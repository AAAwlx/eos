#include "printk.h"
#include "io.h"
#include "init.h"
#include "memory.h"
#include"thread.h"
#include"process.h"
void u_prog_a(void);
void u_prog_b(void);
int test_var_a = 0, test_var_b = 0;
void k_thread_a(void*);
void k_thread_b(void*);
int main()
{
    init_all();
    intr_enable();
    console_put_str(" main_pid:0x");
   console_put_int(sys_getpid());
   console_put_char('\n');
    thread_start("k_thread_a", 31, k_thread_a, "argA ");
    thread_start("k_thread_b", 31, k_thread_b, "argB ");
   process_execute(u_prog_a, "user_prog_a");
   process_execute(u_prog_b, "user_prog_b");
    while(1);
}
void k_thread_a(void* arg) {     
/* 用void*来通用表示参数,被调用的函数知道自己需要什么类型的参数,自己转换再用 */
   char* para = arg;
   console_put_str(" thread_a_pid:0x");
   console_put_int(sys_getpid());
   console_put_char('\n');
   while(1);
}

/* 在线程中运行的函数 */
void k_thread_b(void* arg) {     
/* 用void*来通用表示参数,被调用的函数知道自己需要什么类型的参数,自己转换再用 */
   char* para = arg;
   console_put_str(" thread_b_pid:0x");
   console_put_int(sys_getpid());
   console_put_char('\n');
   while(1);
}
void u_prog_a(void) {
   printf("prog_a_pid:0x%x\n", getpid());
   while(1);
}

/* 测试用户进程 */
void u_prog_b(void) {
   printf("prog_b_pid:0x%x\n", getpid());
   while (1);
}