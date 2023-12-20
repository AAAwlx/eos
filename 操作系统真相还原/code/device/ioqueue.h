#ifndef __DEVICE_IOQUEUE_H
#define __DEVICE_IOQUEUE_H
#include "../lib/stdint.h"
#include "thread.h"
#include "sysnc.h"
#define bufsize 64
struct ioqueue
{
    struct lock buf_lock;
    struct task_pcb *consumer;
    struct task_pcb *producer;
    char buf[bufsize];
    int32_t head;
    int32_t tail;
};
void ioqueue_init(struct ioqueue* ioq);
bool ioq_full(struct ioqueue* ioq);
bool ioq_empty(struct ioqueue* ioq);
char ioq_getchar(struct ioqueue* ioq);
void ioq_putchar(struct ioqueue* ioq, char byte);
#endif