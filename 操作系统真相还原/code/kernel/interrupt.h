#ifndef __KERNEL_INTERRUPT_H
#define __KERNEL_INTERRUPT_H
#include "stdint.h"
typedef void* intr_handler;//程序的地址
void idt_init(void);
void register_hsndler(uint32_t inrtnum, intr_handler fucntion);
enum intr_status  // 状态的结构体
{
    INTR_OFF,
    INTR_NO
};
enum intr_status intr_get_status(void);
enum intr_status intr_set_status (enum intr_status);
enum intr_status intr_enable (void);
enum intr_status intr_disable (void);
#endif
