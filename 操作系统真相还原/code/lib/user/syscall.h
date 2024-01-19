#ifndef __LIB_USER_SYSCALL_H
#define __LIB_USER_SYSCALL_H
#include "thread.h"
//该结构体中定义了系统调用的调用号
enum SYSCALL_NR {
   SYS_GETPID,
   SYS_WRITE,
   SYS_MALLOC,
   SYS_FREE,
   SYS_FORK,
   SYS_READ,
   SYS_PUTCHAR,
   SYS_CLEAR,
   SYS_GETCWD,
   SYS_OPEN,
   SYS_CLOSE,
   SYS_LSEEK,
   SYS_UNLINK,
   SYS_MKDIR,
   SYS_OPENDIR,
   SYS_CLOSEDIR,
   SYS_CHDIR,
   SYS_RMDIR,
   SYS_READDIR,
   SYS_REWINDDIR,
   SYS_STAT,
   SYS_PS,
   SYS_EXECV,
   SYS_EXIT,
   SYS_WAIT,
   SYS_PIPE,
   SYS_FD_REDIRECT,
   SYS_HELP
};
uint32_t getpid(void);
uint32_t write(char* str);
#endif
