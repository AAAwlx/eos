#ifndef __THREAD_SYNC_H
#define __THREAD_SYNC_H
#include "list.h"
#include "stdint.h"
#include "thread.h"
struct semaphore {
   uint8_t  value;//信号量的值
   struct   list waiters;//在同一个锁上阻塞的任务队列
};
struct lock {
   struct task_pcb* holder;	    // 当前锁的持有者
   struct semaphore semaphore;	    // 用二元信号量实现锁
   uint32_t holder_repeat_nr;		    // 锁的持有者重复申请锁的次数
};
void sema_init(struct semaphore* psema, uint8_t value); //初始化信号量
void sema_down(struct semaphore* psema);//减少信号量
void sema_up(struct semaphore* psema);//增加信号量
void lock_init(struct lock* plock);//初始化锁
void lock_acquire(struct lock* plock);//加锁
void lock_release(struct lock* plock);//解锁
# endif