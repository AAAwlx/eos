#include "io.h"
#include "printf.h"
#include "time.h"
#define IRQ0_FREQUENCY	   1
#define INPUT_FREQUENCY	   1193180//频率设置
#define COUNTER0_VALUE	   INPUT_FREQUENCY / IRQ0_FREQUENCY
#define CONTRER0_PORT	   0x40//计数器对应的端口
#define COUNTER0_NO	   0//选择的计数器的标号
#define COUNTER_MODE	   2//
#define READ_WRITE_LATCH   3//读写的模式
#define PIT_CONTROL_PORT   0x43//控制字端口 
static void frequency_set(uint8_t counter_port,uint8_t rwl,uint8_t counter_mode,uint8_t counter_no,uint16_t counter_value)
{
    outb(PIT_CONTROL_PORT, ((uint8_t)counter_no << 6 | (uint8_t)rwl << 4 | (uint8_t)counter_mode << 1));
    outb(counter_port, (uint8_t)counter_value);//先写低八位
    outb(counter_port, (uint8_t)counter_value >> 8);//再写高八位
}
void init_time()
{
    put_str();
    frequency_set(CONTRER0_PORT, READ_WRITE_LATCH, COUNTER_MODE, COUNTER0_NO,COUNTER0_VALUE);
    put_str();
}