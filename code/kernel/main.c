#include "console.h"
#include "fs.h"
#include "init.h"
#include "interrupt.h"
#include "memory.h"
#include "print.h"
#include"printk.h"
#include "process.h"
#include "stdio.h"
#include "syscall-init.h"
#include "syscall.h"
#include "thread.h"
#include"shell.h"
#include"dir.h"
#include"keyboard.h"
void init(void);
/*int main(void) {
   put_str("I am kernel\n");
   init_all();
   uint32_t fd = sys_open("/file1", O_RDWR);
   printf("fd:%d\n", fd);
   sys_write(fd, "hello,world\n", 12);
   sys_close(fd);
   printf("%d closed now\n", fd);
   while(1);
   return 0;
}*/
int main(void) {
   put_str("I am kernel\n");
   init_all();
   uint32_t fd = sys_open("/file1", O_RDWR);
   printf("open /file1, fd:%d\n", fd);
   char buf[64] = {0};
   int read_bytes = sys_read(fd, buf, 18);
   printf("1_ read %d bytes:\n%s\n", read_bytes, buf);

   memset(buf, 0, 64);
   read_bytes = sys_read(fd, buf, 6);
   printf("2_ read %d bytes:\n%s", read_bytes, buf);

   memset(buf, 0, 64);
   read_bytes = sys_read(fd, buf, 6);
   printf("3_ read %d bytes:\n%s", read_bytes, buf);

   printf("________  close file1 and reopen  ________\n");
   sys_close(fd);
   fd = sys_open("/file1", O_RDWR);
   memset(buf, 0, 64);
   read_bytes = sys_read(fd, buf, 24);
   printf("4_ read %d bytes:\n%s", read_bytes, buf);

   sys_close(fd);
   while(1);
   return 0;
}

/* init进程 */
void init(void)
{
    uint32_t ret_pid = fork();
    if (ret_pid)
    { // 父进程
        while (1)
            ;
    }
    else
    { // 子进程
        my_shell();
    }
    PANIC("init: should not be here");
}
