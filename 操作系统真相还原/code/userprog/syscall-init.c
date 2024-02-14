#include "syscall-init.h"
#include "../lib/user/syscall.h"
#include "stdint.h"
#include "print.h"
#include "thread.h"
#include "console.h"
#include"fs.h"
#include "string.h"
#include "../kernel/memory.h"
#define syscall_nr 32
typedef void* syscall;
syscall syscall_table[syscall_nr];
uint32_t sys_getpid(void)
{
    if (running_thread()->pgdir)
    {
        put_str(running_thread()->name);
    }

    return running_thread()->pid;
}
void syscall_init()
{
    put_str("syscall_init stary\n");
    syscall_table[SYS_GETPID] = sys_getpid;
    syscall_table[SYS_WRITE] = sys_write;
    syscall_table[SYS_MALLOC] = sys_malloc;
    syscall_table[SYS_FREE] = sys_free;
    put_str("syscall_init done\n");
}