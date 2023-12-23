#include "io.h"
#include "print.h"
#include "timer.h"
#include "interrupt.h"
#include "thread.h"
#include"debug.h"
#define IRQ0_FREQUENCY	   1
#define INPUT_FREQUENCY	   1193180//频率设置
#define COUNTER0_VALUE	   INPUT_FREQUENCY / IRQ0_FREQUENCY
#define CONTRER0_PORT	   0x40//计数器对应的端口
#define COUNTER0_NO	   0//选择的计数器的标号
#define COUNTER_MODE	   2//
#define READ_WRITE_LATCH   3//读写的模式
#define PIT_CONTROL_PORT   0x43//控制字端口
uint32_t sys_ticks;
static void frequency_set(uint8_t counter_port,uint8_t rwl,uint8_t counter_mode,uint8_t counter_no,uint16_t counter_value) {
    outb(PIT_CONTROL_PORT, ((uint8_t)counter_no << 6 | (uint8_t)rwl << 4 | (uint8_t)counter_mode << 1));
    outb(counter_port, (uint8_t)counter_value);//先写低八位
    outb(counter_port, (uint8_t)counter_value >> 8);//再写高八位
}
static void intr_timer_handler(void)
{
    //put_str("timeintr\n");
    struct task_pcb* cur = running_thread();
    ASSERT(cur->stack_magic == 0x12345678);
    cur->elapsed_ticks++;
    //sys_ticks++;
    if (cur->ticks==0)
    {
        schedule();
    }else
    {
        cur->ticks--;
    }
}
void init_time()
{
    put_str(" timer init start\n ");
    frequency_set(CONTRER0_PORT, READ_WRITE_LATCH, COUNTER_MODE, COUNTER0_NO,COUNTER0_VALUE);
    register_hsndler(0x20,intr_timer_handler);
    put_str("timer_init done\n");
}