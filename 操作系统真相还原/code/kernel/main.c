#include "print.h"
#include "io.h"
#include "init.h"
#include "memory.h"
#include"thread.h"
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
    while(1) {
        put_str("Main ");
    };
   /*void* addr = get_kernel_pages(3);
    put_str("\n get_kernel_page start vaddr is ");
    put_int((uint32_t)addr);
    put_str("\n");
    void* addr2 = get_kernel_pages(2);
    put_str("\n get_kernel_page start vaddr2 is ");
    put_int((uint32_t)addr2);
    put_str("\n");
    while(1);
    return 0;intr_enable();	// 打开中断,使时钟中断起作用
   while(1) {
      put_str("Main ");
   };
   return 0;*/
   /**/
}
void k_thread_a(void* arg) {     
/* 用void*来通用表示参数,被调用的函数知道自己需要什么类型的参数,自己转换再用 */
   char* para = arg;
   while(1) {
      put_str(para);
   }
}

/* 在线程中运行的函数 */
void k_thread_b(void* arg) {     
/* 用void*来通用表示参数,被调用的函数知道自己需要什么类型的参数,自己转换再用 */
   char* para = arg;
   while(1) {
      put_str(para);
   }
}