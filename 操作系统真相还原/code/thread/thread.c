#include "thread.h"
#include "stdint.h"
#include "string.h"
#include "global.h"
#include "debug.h"
#include "interrupt.h"
#include "printk.h"
#include "memory.h"
#include "process.h"
#include "sysnc.h"
struct task_struct* idle_thread;
struct task_pcb* main_thread;    // 主线程PCB
struct list general_list;	    // 就绪队列
struct list all_list;	    // 所有任务队列
struct lock lock_pid;		    // 分配pid锁
static struct list_node* thread_tag;// 用于保存队列中的线程结点

extern void switch_to(struct task_pcb* cur, struct task_pcb* next);

/* 获取当前线程pcb指针 */
struct task_pcb* running_thread() {
   uint32_t esp; 
   asm ("mov %%esp, %0" : "=g" (esp));
  /* 取esp整数部分即pcb起始地址 */
   return (struct task_pcb*)(esp & 0xfffff000);
}

/* 由kernel_thread去执行function(func_arg) */
static void kernel_thread(thread_func* function, void* func_arg) {
/* 执行function前要开中断,避免后面的时钟中断被屏蔽,而无法调度其它线程 */
   intr_enable();
   function(func_arg); 
}

/* 分配pid */
static pid_t allocate_pid(void) {
   static pid_t next_pid = 0;  // 设置静态变量
    lock_acquire(&lock_pid);
    next_pid++;
    lock_release(&lock_pid);
    return next_pid;
}

/* 初始化线程栈thread_stack,将待执行的函数和参数放到thread_stack中相应的位置 */
void thread_create(struct task_pcb* pthread,
                   thread_func function,
                   void* func_arg) {
    /* 先预留中断使用栈的空间,可见thread.h中定义的结构 */
    pthread->self_kstack -= sizeof(struct intr_stack);

    /* 再留出线程栈空间,可见thread.h中定义 */
    pthread->self_kstack -= sizeof(struct thread_stack);
    struct thread_stack* kthread_stack =
        (struct thread_stack*)pthread->self_kstack;
    kthread_stack->eip = kernel_thread;
    kthread_stack->function = function;
    kthread_stack->func_arg = func_arg;
    kthread_stack->ebp = kthread_stack->ebx = kthread_stack->esi =
        kthread_stack->edi = 0;
}
/* 初始化线程基本信息 */
void init_thread(struct task_pcb* pthread, char* name, int prio) {
    memset(pthread, 0, sizeof(*pthread));
    pthread->pid = allocate_pid();
    strcpy(pthread->name, name);

    if (pthread == main_thread) {
        /* 由于把main函数也封装成一个线程,并且它一直是运行的,故将其直接设为TASK_RUNNING
         */
        pthread->status = TASK_RUNNING;
    } else {
        pthread->status = TASK_READY;
    }

    /* self_kstack是线程自己在内核态下使用的栈顶地址 */
    pthread->self_kstack = (uint32_t*)((uint32_t)pthread + PG_SIZE);
    pthread->priority = prio;
    pthread->ticks = prio;
    pthread->elapsed_ticks = 0;
    pthread->pgdir = NULL;
    pthread->stack_magic = 0x12345678;  // 自定义的魔数
}

/* 创建一优先级为prio的线程,线程名为name,线程所执行的函数是function(func_arg) */
struct task_pcb* thread_start(char* name,
                              int prio,
                              thread_func function,
                              void* func_arg) {
    /* pcb都位于内核空间,包括用户进程的pcb也是在内核空间 */
    struct task_pcb* thread = get_kernel_pages(1);

    init_thread(thread, name, prio);
    thread_create(thread, function, func_arg);

    /* 确保之前不在队列中 */
    ASSERT(!elem_find(&general_list, &thread->general_tag));
    /* 加入就绪线程队列 */
    list_append(&general_list, &thread->general_tag);

    /* 确保之前不在队列中 */
    ASSERT(!elem_find(&all_list, &thread->all_list_tag));
    /* 加入全部线程队列 */
    list_append(&all_list, &thread->all_list_tag);

    return thread;
}

/* 将kernel中的main函数完善为主线程 */
static void make_main_thread() {
    main_thread = running_thread();
    init_thread(main_thread, "main", 31);
    ASSERT(!elem_find(&all_list, &main_thread->all_list_tag));
    list_append(&all_list, &main_thread->all_list_tag);
}
static void idle(void* arg UNUSED) {
    while (1) {
        thread_lock(TASK_BLOCKED);
        // 执行 hlt 时必须要保证目前处在开中断的情况下
        // (不然程序就会挂在下面那条指令上)
        asm volatile("sti; hlt" : : : "memory");
    }
}
/* 实现任务调度 */
void schedule() {
    ASSERT(intr_get_status() == INTR_OFF);
    struct task_pcb* cur = running_thread();
    if (cur->status==TASK_RUNNING) {//如果当前任务是运行中的状态
        ASSERT(!elem_find(&general_list, &cur->general_tag));
        list_append(&general_list, &cur->general_tag); // 将任务加入到就绪的队尾
        cur->ticks = cur->priority;
        cur->status = TASK_READY;
    } else {//如果线程在等待某个事件

    }
	if (list_empty(&general_list)) {
    thread_unlock(idle_thread);
  }
	
    ASSERT(!list_empty(&general_list));
    thread_tag = NULL;
    thread_tag = list_pop(&general_list);
    struct task_pcb* next = elem2entry(struct task_pcb , general_tag, thread_tag);
    next->status = TASK_RUNNING;
    process_activate(next);
    switch_to(cur, next);
}
void thread_yield(){}
/* 当前线程将自己阻塞,标志其状态为stat. */
void thread_lock(enum task_stat stat) {
    ASSERT(((stat == TASK_BLOCKED) || (stat == TASK_WAITING) ||
            (stat == TASK_HANGING)));  // 判断当前要阻塞线程的状态
    enum intr_status intr = intr_disable();   // 关中断
    struct task_pcb* cur = running_thread();  // 获取当前线程的pcb
    cur->status = stat;
    schedule();
    intr_set_status(intr);
}
// 唤醒被阻塞线程
void thread_unlock(struct task_pcb* pthread) {
    ASSERT(((pthread->status == TASK_BLOCKED) ||
            (pthread->status == TASK_WAITING) ||
            (pthread->status == TASK_HANGING)));  // 判断线程是否被锁住
    enum intr_status intr = intr_disable();       // 关中断
    if (pthread->status != TASK_READY) {
        ASSERT(!elem_find(
            &general_list,
            &pthread->general_tag));  // 再次判断，若在就绪队列中则报错
        if (elem_find(&general_list, &pthread->general_tag)) {
            PANIC("thread_unblock: blocked thread in ready_list\n");
        }
        pthread->status = TASK_READY;  // 更改线程状态
        list_push(
            &general_list,
            &pthread->general_tag);  // 将当前任务放入就绪队首，尽快得到调度
    }
    intr_set_status(intr);
}
/* 初始化线程环境 */
void thread_init(void) {
   put_str("thread_init start\n");
   list_init(&general_list);
   list_init(&all_list);
   lock_init(&lock_pid);
/* 将当前main函数创建为线程 */
   make_main_thread();
   idle_thread = thread_start("idle", 10, idle, NULL);
   put_str("thread_init done\n");
}

