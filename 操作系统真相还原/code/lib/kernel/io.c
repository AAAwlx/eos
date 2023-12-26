#include "./io.h"
#include "../lib/stdint.h"
//向端口写入单个数据
inline void outb(uint16_t port,uint8_t value){
    asm volatile("outb %b0,%w1" 
    :
    :"a"(value), "Nd"(port));
}
//向端口写入连续的一串数据
inline void outw(uint16_t port,const void* addr,uint32_t size){
    asm volatile("cld;rep outsw " 
    : "+S"(addr), "+c"(size)
    :"d"(port));
}
//从端口读出单个数据
 inline uint8_t inb(uint16_t port) {
  uint8_t ret;
  asm volatile("inb %1,%0" : "=a"(ret) : "dN"(port));
  return ret;
}

//从端口读出连续的一串数据
 inline void inw(uint16_t port,const void* addr,uint32_t size){
    asm volatile("cld;rep insw " 
    : "+D"(addr), "+c"(size)
    : "d"(port)
    :"memory");
    //先是输出约束后是输入约束 +号表示先读入再写入
}

