#ifndef __KERNEL_INTERRUPT_H
#define __KERNEL_INTERRUPT_H
#include "stdint.h"
typedef void* intr_handler;//程序的地址
void idt_init(void);
enum intr_status //状态的结构体
{ 
    INTR_OFF,
    INTR_NO
};
#endif
