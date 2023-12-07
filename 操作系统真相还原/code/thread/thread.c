#include"thread.h"
#include"memory.h"
#include"debug.h"
struct list* general_list;//就绪任务队列
struct list* all_list;//全部任务队列
struct task_pcb* main_thread;//如果是主线程  
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
void main_thread_init()
{
    main_thread=running_thread();
    init_thread(main_thread, "main", 31);
    ASSERT(!elem_find(&all_list, &main_thread->general_tag));
    list_append(&all_list, &main_thread->general_tag);
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