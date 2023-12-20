#include "sysnc.h"
#include "debug.h"
#include "global.h"
#include "interrupt.h"
#include "list.h"
void sema_init(struct semaphore* psema, uint8_t value)//初始化信号量
{
    psema->value = value;
    list_init(&psema->waiters);
}
void lock_init(struct lock* plock)//初始化锁
{
    plock->holder = NULL;
    plock->holder_repeat_nr = 0;
    sema_init(&plock->semaphore, 0);
}
void sema_down(struct semaphore* psema)//减少信号量
{
    enum intr_status intr = intr_disable();
    while (psema->value==0)
    {
        ASSERT(elem_find(&psema->waiters, &(running_thread()->general_tag)));
        
        if (elem_find(&psema->waiters, &(running_thread()->general_tag)))
        {
            PANIC("sema_down: thread blocked has been in waiters_list\n");
        }
        thread_lock(TASK_BLOCKED);
        list_append(&psema->waiters, &(running_thread()->general_tag));
    }
    ASSERT(psema->value == 0);
    psema->value--;
    intr_set_status(intr);
}
void sema_up(struct semaphore* psema)//增加信号量
{
    enum intr_status intr = intr_disable();
    if (psema->value==0)
    {
        if (!list_empty(&psema->waiters))
        {
            struct task_pcb* pthread = elem2entry(struct task_pcb, general_tag, list_pop(&psema->waiters));
            thread_unlock(pthread);
        }

    }
    psema->value++;
    intr_set_status(intr);
}
void lock_acquire(struct lock* plock)//加锁
{
    if (plock->holder!=running_thread())
    {
        plock->holder = running_thread();
        sema_down(&plock->semaphore);
        ASSERT(plock->holder_repeat_nr == 0);
        plock->holder_repeat_nr = 1;
    } else {//如果是锁的持有者直接加一
        plock->holder_repeat_nr++;
    }
}
void lock_release(struct lock* plock)//解锁 
{
    ASSERT(plock->holder == running_thread());//判断当前要执行释放操作的线程是否是锁的持有者，若不是报错中断
    if (plock->holder_repeat_nr>1)//若持有数量大于1直接--不需要对信号量进行操作
    {
        plock->holder_repeat_nr--;
        return;
    }
    ASSERT(plock->holder_repeat_nr == 1);//判断当前锁的持有量是否为1,若小于1报错中断
    plock->holder = NULL;
    plock->holder_repeat_nr = 0;
    sema_up(&plock->semaphore);
    
}