#include "console.h"
#include "fs.h"
#include "init.h"
#include "interrupt.h"
#include "memory.h"
#include "print.h"
#include "process.h"
#include "stdio.h"
#include "syscall-init.h"
#include "syscall.h"
#include "thread.h"
void k_thread_a(void*);
void k_thread_b(void*);
void u_prog_a(void);
void u_prog_b(void);

int main(void) {
    put_str("I am kernel\n");
    init_all(); 
    intr_enable();
    /*process_execute(u_prog_a, "u_prog_a");
    process_execute(u_prog_b, "u_prog_b");
    thread_start("k_thread_a", 31, k_thread_a, "I am thread_a");
    thread_start("k_thread_b", 31, k_thread_b, "I am thread_b");*/
    uint32_t fd = sys_open("/file1",O_CREAT);
    printk("fd:%d\n", fd);
    //sys_write(fd, "hello,world\n", 12);
    sys_close(fd);
    printk("%d closed now\n", fd);
    while(1);
    return 0;
}

/* 在线程中运行的函数 */
void k_thread_a(void* arg) {
    console_put_str("k_thread_a\n");
    void* addr1 = sys_malloc(256);
    console_put_str(" thread_a malloc addr:0x");
    console_put_int((int)addr1);
    void* addr2 = sys_malloc(255);
    console_put_char(',');
    console_put_int((int)addr2);
    void* addr3 = sys_malloc(254);
    console_put_char(',');
    console_put_int((int)addr3);
    console_put_char('\n');
    int cpu_delay = 100000;
    while (cpu_delay-- > 0)
        ;
    sys_free(addr1);
    sys_free(addr2);
    sys_free(addr3);
    while (1)
        ;
}

/* 在线程中运行的函数 */
void k_thread_b(void* arg) {
    console_put_str("k_thread_b\n");
    void* addr1 = sys_malloc(256);
    void* addr2 = sys_malloc(255);
    void* addr3 = sys_malloc(254);
    console_put_str(" thread_b malloc addr:0x");
    console_put_int((int)addr1);
    console_put_char(',');
    console_put_int((int)addr2);
    console_put_char(',');
    console_put_int((int)addr3);
    console_put_char('\n');

    int cpu_delay = 100000;
    while (cpu_delay-- > 0)
        ;
    sys_free(addr1);
    sys_free(addr2);
    sys_free(addr3);
    while (1)
        ;
}

/* 测试用户进程 */
void u_prog_a(void) {
    printf("u_prog_a\n");
    void* addr1 = malloc(256);
    void* addr2 = malloc(255);
    void* addr3 = malloc(254);
    printf(" prog_a malloc addr:0x%x,0x%x,0x%x\n", (int)addr1, (int)addr2,
           (int)addr3);

    int cpu_delay = 100000;
    while (cpu_delay-- > 0)
        ;
    free(addr1);
    free(addr2);
    free(addr3);
    while (1)
        ;
}

/* 测试用户进程 */
void u_prog_b(void) {
    printf("u_prog_b\n");
    void* addr1 = malloc(256);
    void* addr2 = malloc(255);
    void* addr3 = malloc(254);
    printf(" prog_b malloc addr:0x%x,0x%x,0x%x\n", (int)addr1, (int)addr2,
           (int)addr3);

    int cpu_delay = 100000;
    while (cpu_delay-- > 0)
        ;
    free(addr1);
    free(addr2);
    free(addr3);
    while (1)
        ;
}
void init(void) {
  uint32_t ret_pid = fork();
  if (ret_pid) {
    int status;
    int child_pid;
    /* init 在此处不停地回收僵尸进程 */
    while (1) {
      child_pid = wait(&status);
      printf(
          "I`m init, My pid is 1, I recieve a child, It`s pid is %d, status is "
          "%d\n",
          child_pid, status);
    }
  } else {
    my_shell();
  }
  while (1)
    ;
}