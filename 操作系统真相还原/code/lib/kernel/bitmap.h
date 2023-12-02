#ifndef _KERNEL_BITMAP_
#define _KERNEL_BITMAP_
#include"stdint.h"
#include"global.h"
#define BITMAP_MASK 1
struct bitmap
{
    uint32_t bitmap_len;
    uint8_t* bits;
};
void bitmap_init(struct bitmap* btmp);//初始化位图
bool bitmap_scan_test(struct bitmap* btmp, uint32_t bit_idx);//检测当前位是否是1
int bitmap_scan(struct bitmap* btmp, uint32_t cnt);//寻找连续的没有被标记使用的内存
void bitmap_set(struct bitmap* btmp, uint32_t bit_idx, int8_t value);//将位图的第bit_idx位设置为value
#endif