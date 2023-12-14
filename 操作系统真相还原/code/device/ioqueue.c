#include "ioqueue.h"
#include "interrupt.h"
#include "global.h"
#include "debug.h"
void ioqueue_init(struct ioqueue* ioq)
{
    lock_init(&ioq->buf_lock);
    ioq->head = ioq->tail = 0;
    ioq->consumer = ioq->producer = NULL;
}
static int32_t next_pos(int32_t pos) {
   return (pos + 1) % bufsize; 
}
bool ioq_full(struct ioqueue* ioq)
{
    ASSERT(intr_get_status() == INTR_OFF);
    return next_pos(ioq->head) == ioq->tail;
}
bool ioq_empty(struct ioqueue* ioq)
{
    ASSERT(intr_get_status() == INTR_OFF);
    return ioq->head == ioq->tail;
}
static void ioq_wait(struct task_pcb** waiter)
{
    ASSERT(*waiter == NULL && waiter != NULL);
    *waiter = running_thread();
    thread_lock(TASK_BLOCKED);
}
static void wakeup(struct task_pcb** waiter)
{
    ASSERT(*waiter != NULL);
    thread_unlock(waiter);
    *waiter = NULL;
}
char ioq_getchar(struct ioqueue* ioq)
{
    ASSERT(intr_get_status() == INTR_OFF);
    while (ioq_empty(ioq))//如果缓冲区为空
    {
        lock_acquire(&ioq->buf_lock);	 
        ioq_wait(&ioq->consumer);
        lock_release(&ioq->buf_lock);
    }
    char byte;
    ioq->head = next_pos(ioq->head);
    byte = ioq->buf[ioq->head];
    if (ioq->producer!=NULL)
    {
        wakeup(&ioq->producer);
    }
    return byte;
}
void ioq_putchar(struct ioqueue* ioq, char byte)
{
    ASSERT(intr_get_status() == INTR_OFF);
    while (ioq_full(ioq))
    {
        lock_acquire(&ioq->buf_lock);	 
        ioq_wait(&ioq->producer);
        lock_release(&ioq->buf_lock);
    }
    ioq->buf[ioq->tail] = byte;
    ioq->tail = next_pos(ioq->tail);
    if (ioq->consumer!=NULL)
    {
        wakeup(&ioq->producer);
    }
}