#ifndef __DEVICE_CONSOLE_H
#define __DEVICE_CONSOLE_H
#include "../lib/stdint.h"
#include"print.h"
void console_init(void);
void console_acquire(void);
void console_release(void);
void console_put_str(char* str);
void console_put_char(uint8_t char_asci);
void console_put_int(uint32_t num);
void console_str_color(char* c, real_color_t fore);
void sys_putchar(uint8_t c);
#endif