#ifndef __LIB_KERNEL_PRINT_H
#define __LIB_KERNEL_PRINT_H
#include "../stdint.h"
typedef enum real_color {
  rc_black = 0,
  rc_blue = 1,
  rc_green = 2,
  rc_cyan = 3,
  rc_red = 4,
  rc_magenta = 5,
  rc_brown = 6,
  rc_light_grey = 7,
  rc_dark_grey = 8,
  rc_light_blue = 9,
  rc_light_green = 10,
  rc_light_cyan = 11,
  rc_light_red = 12,
  rc_light_magenta = 13,
  rc_light_brown = 14,  // yellow
  rc_white = 15
} real_color_t;

void put_char(uint8_t char_asci);
void put_str(char * str);
void put_int(uint32_t num);
void set_cursor();
void put_str_color(char * str, real_color_t back, real_color_t fore);
void cls_screen(void);
#endif