#ifndef __THREAD_THREAD_H
#define __THREAD_THREAD_H
#include "../kernel/memory.h"
#include "../lib/stdint.h"
#include "list.h"
#define MAX_FILES_OPEN_PER_PROC 8
typedef void thread_func(void*);
typedef int16_t pid_t;
enum task_stat {
    TASK_RUNNING,  // 运行
    TASK_READY,    // 就绪
    TASK_BLOCKED,  // 锁
    TASK_WAITING,  // 等待
    TASK_HANGING,  // 挂起
    TASK_DIED      // 死亡
};
// 中断时保存寄存器上下文
struct intr_stack {
    uint32_t vec_no;  // kernel.S 宏VECTOR中push %1压入的中断号
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t
        esp_dummy;  // 虽然pushad把esp也压入,但esp是不断变化的,所以会被popad忽略
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
    uint32_t gs;
    uint32_t fs;
    uint32_t es;
    uint32_t ds;
    uint32_t err_code;  // err_code会被压入在eip之后
    void (*eip)(void);  // 指令指针
    uint32_t cs;
    uint32_t eflags;
    void* esp;  // 特权级更换时需要切换特级栈
    uint32_t ss;
};
// 任务切换时用到的结构体，（内核栈）
struct thread_stack {
    // 根据ABI的约定，将寄存器压栈保存
    uint32_t ebp;
    uint32_t ebx;
    uint32_t edi;
    uint32_t esi;
    // 以下是线程第一次被调度上cpu时需要初始化的信息
    void (*eip)(
        thread_func* func,
        void*
            func_arg);  // 函数指针，第一次被调度时指向kernel_thread函数，再由kernel_thread函数调用线程执行的目的函数
    void(*unused_retaddr);  // 用来占位，充当调用kernel_thread函数的返回地址
    thread_func* function;  // 由Kernel_thread所调用的函数名
    void* func_arg;         // 由Kernel_thread所调用的函数所需的参数
};
struct task_pcb {
    uint32_t* self_kstack;  // 各内核线程都用自己的内核栈
    pid_t pid;
    enum task_stat status;
    char name[16];
    uint8_t priority;
    uint8_t ticks;  // 每次在处理器上执行的时间嘀嗒数

    /* 此任务自上cpu运行后至今占用了多少cpu嘀嗒数,
     * 也就是此任务执行了多久*/
    uint32_t elapsed_ticks;

    /* general_tag的作用是用于线程在一般的队列中的结点 */
    struct list_node general_tag;
    int32_t fd_table[MAX_FILES_OPEN_PER_PROC];  // 文件描述符数组
    /* all_list_tag的作用是用于线程队列thread_all_list中的结点 */
    struct list_node all_list_tag;

    uint32_t* pgdir;                      // 进程自己页表的虚拟地址
    struct virtual_addr userprog_vaddar;  // 进程的虚拟内存管理结构体
    struct mem_block_desc u_block_descs[DESC_CNT];
    uint32_t cwd_inode_nr;  // 进程所在的工作目录的inode编号
    int16_t parent_pid;     // 父进程的pid
    int8_t exit_status;    //进程结束调用时传入自己的返回值
    uint32_t stack_magic;  // 用这串数字做栈的边界标记,用于检测栈的溢出
};

extern struct list general_list;
extern struct list all_list;

void thread_create(struct task_pcb* pthread,
                   thread_func function,
                   void* func_arg);
void init_thread(struct task_pcb* pthread, char* name, int prio);
struct task_pcb* thread_start(char* name,int prio,thread_func function,void* func_arg);
struct task_pcb* running_thread(void);
void schedule(void);
void thread_init(void);
void thread_unlock(struct task_pcb* pthread);
void thread_lock(enum task_stat stat);
void thread_yield();
pid_t fork_pid(void);
void release_pid(pid_t pid);
void thread_exit(struct task_pcb* thread_over, bool need_schedule);
struct task_pcb* pid2thread(int32_t pid);
void sys_ps(void);
#endif