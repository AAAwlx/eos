#include "printk.h"
#include "io.h"
#include "init.h"
#include "memory.h"
#include"thread.h"
void u_prog_a(void);
void u_prog_b(void);
int test_var_a = 0, test_var_b = 0;
void k_thread_a(void*);
void k_thread_b(void*);
int main()
{
    put_str("I'm kernel\n");
    init_all();
    put_str("init_down\n");
    intr_enable();
    put_str("intr_on\n");
    thread_start("k_thread_a", 31, k_thread_a, "argA ");
    thread_start("k_thread_b", 8, k_thread_b, "argB ");
   process_execute(u_prog_a, "user_prog_a");
   process_execute(u_prog_b, "user_prog_b");
    while(1) {
    };
}
void k_thread_a(void* arg) {     
/* 用void*来通用表示参数,被调用的函数知道自己需要什么类型的参数,自己转换再用 */
   char* para = arg;
   while(1) {
      //printk("%s :%d\n", arg, test_var_a);
   }
}

/* 在线程中运行的函数 */
void k_thread_b(void* arg) {     
/* 用void*来通用表示参数,被调用的函数知道自己需要什么类型的参数,自己转换再用 */
   char* para = arg;
   while(1) {
      //printk("%s: %d\n", arg, test_var_b);
   }
}
void u_prog_a(void) {
   while(1) {
      test_var_a++;
   }
}

/* 测试用户进程 */
void u_prog_b(void) {
   while(1) {
      test_var_b++;
   }
}