#include"../lib/string.h"
#include"bitmap.h"
#include"debug.h"
void bitmap_init(struct bitmap* btmp)
{
    memset(btmp->bits, 0, btmp->bitmap_len);
}
bool bitmap_scan_test(struct bitmap* btmp, uint32_t bit_idx)//比较当前位是0还是一
{
    uint32_t byte_idx = bit_idx / 8;
    uint32_t bit_old = bit_idx % 8;
    return (btmp->bits[byte_idx] & (BITMAP_MASK << bit_old));
}
int bitmap_scan(struct bitmap* btmp, uint32_t cnt)
{
    //寻找不全为1的八字节
    int i_byte = 0;
    while ((btmp->bits[i_byte] == 0xff)&&(i_byte<btmp->bitmap_len)) {
        i_byte++;
    }
    ASSERT(i_byte < btmp->bitmap_len);
    //找不到不全为1的8字节就直接返回
    if (i_byte == btmp->bitmap_len)
    {
        return -1;
    }
    //在不全为1的八字节中寻找第一位出现的0
    int i_bit = 0;
    while ((uint8_t)(BITMAP_MASK<<i_bit)&btmp->bits[i_byte])
    {
        i_bit++;
    }
    int bit_start = i_byte * 8 + i_bit;//第一个没有被标记使用的地方。
    if(cnt==1)//如果要找的位只有1
    {
        return bit_start;
    }
    uint32_t bit_left = (btmp->bitmap_len * 8 - bit_start);//剩余的bit数量
    uint32_t next_bit = bit_start + 1;
    bit_start = -1;//若没找到count个连续的位图就直接返回-1
    int count = 1;
    while (bit_left-- > 0) {
        if(!bitmap_scan_test(btmp, next_bit)){//如果该位为1
            count++;
        }else{//否则置0
            count=0;
        }
        if (count==cnt)//如果找到了连续的
        {
            bit_start = next_bit - count + 1;//重新计算其实的bit位
            break;
        }
        next_bit++;
    }
    return bit_start;
}
void bitmap_set(struct bitmap* btmp, uint32_t bit_idx, int8_t value) {
   ASSERT((value == 0) || (value == 1));
   uint32_t byte_idx = bit_idx / 8;    // 向下取整用于索引数组下标
   uint32_t bit_odd  = bit_idx % 8;    // 取余用于索引数组内的位

/* 一般都会用个0x1这样的数对字节中的位操作,
 * 将1任意移动后再取反,或者先取反再移位,可用来对位置0操作。*/
   if (value) {		      // 如果value为1
      btmp->bits[byte_idx] |= (BITMAP_MASK << bit_odd);
   } else {		      // 若为0
      btmp->bits[byte_idx] &= ~(BITMAP_MASK << bit_odd);
   }
}
/*void bitmap_set(struct bitmap* btmp, uint32_t bit_idx, int8_t value)//将位图的第bit_idx位设置为value
{
    ASSERT((value == 0) || (value == 1));
    uint32_t i_byte = bit_idx / 8;
    uint32_t i_bit = bit_idx % 8;
    if (value)//1
    {
        btmp->bits[i_byte] |= (BITMAP_MASK << i_bit);//将
    } else {  // 0
        btmp->bits[i_byte] &= ~(BITMAP_MASK << i_bit);
    }
}*/