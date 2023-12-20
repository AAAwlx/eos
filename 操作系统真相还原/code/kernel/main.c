#include "print.h"
#include "io.h"
#include "init.h"
#include "memory.h"
int main()
{
    put_str("I'm kernel\n");
    init_all();
    asm volatile("sti");	     // 为演示中断处理,在此临时开中断
    while(1);
   /* void* addr = get_kernel_pages(3);
    put_str("\n get_kernel_page start vaddr is ");
    put_int((uint32_t)addr);
   put_str("\n");
   void* addr2 = get_kernel_pages(2);
   put_str("\n get_kernel_page start vaddr2 is ");
   put_int((uint32_t)addr2);
   put_str("\n");
   while(1);
   return 0;*/
}