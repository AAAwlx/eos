#include"thread.h"
#include"memory.h"
#include"debug.h"
#include"interrupt.h"
#include "print.h"
struct list* general_list;//就绪任务队列
struct list* all_list;//全部任务队列
struct task_pcb* main_thread;//如果是主线程  
static struct list_node* thread_tag;// 用于临时保存队列中的线程结点
static struct list_node* general_tag;
//获取当前函数pcb块的起始地址
struct task_pcb* running_thread()
{
    uint32_t esp;//用来保存esp 的值
    asm("mov %%esp,%0" : "=g"(esp));
    return (struct task_pcb*)(esp & 0xfffff000);//将虚拟地址的最后10位页内偏移去掉就能的到pcb页开始的地址。整页分配
}

void init_thread(struct task_pcb* pthread, char* name, int prio) {
    memset(pthread, 0, PG_SIZE);
    strcpy(pthread->name, name);
    if (pthread==main_thread)
    {
        pthread->status = TASK_RUNNING;
    }else{
        pthread->status = TASK_READY;
    }
    pthread->self_kstack = (uint32_t*)(pthread + PG_SIZE);//将栈指针指向pcb的顶端
    pthread->priority = prio;
    pthread->ticks = prio;
    pthread->elapsed_ticks = 0;
    pthread->pgdir = NULL;
    pthread->stack_magic = 0x12345678;
}
static void kernel_thread(thread_func* function, void* func_arg) {
/* 执行function前要开中断,避免后面的时钟中断被屏蔽,而无法调度其它线程 */
   intr_enable();
   function(func_arg); 
}
void thread_create(struct task_pcb* pthread, thread_func function, void* func_arg)
{
    pthread->self_kstack -= sizeof(struct intr_stack);//为中断栈保留内存空间
    pthread->self_kstack -= sizeof(struct thread_stack);//为内核栈保留空间
    struct thread_stack* ts = pthread->self_kstack;
    ts->eip = kernel_thread;
    ts->function = function;
    ts->func_arg = func_arg;
    ts->ebp = 0;
    ts->ebx = 0;
    ts->esi = 0;
    ts->edi = 0;
}
struct task_pcb* thread_start(char* name, int prio, thread_func function, void* func_arg)
{
    struct task_pcb * thread= get_kernel_pages(1);//给pcb分配一块内存用来储存
    init_thread(thread, name, prio);//初始化pcb块的基本信息
    thread_create(thread, function, func_arg);
    //判断是否在就绪队列中
    ASSERT(!elem_find(&general_list, &thread->general_tag));
    list_append(&general_list, &thread->general_tag);
    //判断是否在全部任务队列中
    ASSERT(!elem_find(&all_list, &thread->general_tag));
    list_append(&all_list, &thread->general_tag);
}
static void main_thread_init()
{
    main_thread=running_thread();
    init_thread(main_thread, "main", 31);
    ASSERT(!elem_find(&all_list, &main_thread->general_tag));
    list_append(&all_list, &main_thread->general_tag);
}
void schedule()
{
    ASSERT(get_status() == INTR_OFF);
    struct task_pcb* cur = running_thread();
    if (cur->status==TASK_RUNNING) {//如果当前任务是运行中的状态
        ASSERT(!elem_find(&general_list, &cur->general_tag));
        cur->ticks = cur->priority;
        cur->status = TASK_READY;
        list_append(&general_list, &cur->general_tag);  // 将任务加入到就绪的队尾
    } else {//如果线程在等待某个事件

    }
    ASSERT(!list_empty(&general_list));
    thread_tag = NULL;
    thread_tag = list_pop(&general_list);
    struct task_pcb* next = elem2entry(struct task_pcb , general_tag, thread_tag);
    next->status = TASK_RUNNING;
    switch_to();
}
//阻塞线程自己
void thread_lock(enum task_stat stat)
{
    ASSERT(((stat == TASK_BLOCKED) || (stat == TASK_WAITING) ||
            (stat == TASK_HANGING)));  // 判断当前要阻塞线程的状态
    enum intr_status intr = intr_disable();//关中断
    struct task_pcb *cur = running_thread();//获取当前线程的pcb
    cur->status = stat;
    schedule();
    intr_set_status(intr);
}
//唤醒被阻塞线程
void thread_unlock(struct task_pcb* pthread)
{
    ASSERT(((pthread->status == TASK_BLOCKED) || (pthread->status == TASK_WAITING) || (pthread->status == TASK_HANGING)));//判断线程是否被锁住
    enum intr_status intr = intr_disable();//关中断
    if (pthread->status!=TASK_READY)
    {
        ASSERT(!elem_find(&general_list, &pthread->general_tag));//再次判断，若在就绪队列中则报错
        if (elem_find(&general_list,&pthread->general_tag))
        {
            PANIC("thread_unblock: blocked thread in ready_list\n");
        }
        pthread->status = TASK_READY;//更改线程状态
        list_push(&general_list, &pthread->general_tag);//将当前任务放入就绪队首，尽快得到调度
    }
    intr_set_status(intr);
}
//初始化线程执行的环境
void thread_init(void)
{
    put_str("thread_init start\n");
    list_init(&general_list);
    list_init(&all_list);
/* 将当前main函数创建为线程 */
    main_thread_init();
    put_str("thread_init done\n");
}