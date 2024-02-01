#ifndef __FS_SUPER_BLOCK_H
#define __FS_SUPER_BLOCK_H
#include "stdint.h"
//超级块结构描述磁盘块中的信息
struct super_block {
    uint32_t magic;          // 文件系统的标识类型
    uint32_t sec_cnt;        // 该磁盘分区的总扇区数
    uint32_t inode_cnt;      // 该分区的inode数量
    uint32_t part_lba_base;  // 本分区的lba地址

    uint32_t block_bitmap_lba;    // 块位图的起始地址
    uint32_t block_bitmap_sects;  // 扇区位图本身占用的扇区数量

    uint32_t inode_bitmap_lba;    // i结点位图起始扇区lba地址
    uint32_t inode_bitmap_sects;  // i结点位图占用的扇区数量

    uint32_t inode_table_lba;    // i结点表起始扇区lba地址
    uint32_t inode_table_sects;  // i结点表占用的扇区数量

    uint32_t data_start_lba;  // 数据区开始的第一个扇区号
    uint32_t root_inode_no;   // 根目录所在的I结点号
    uint32_t dir_entry_size;  // 目录项大小

    uint8_t pad[460];  // 加上460字节,凑够512字节1扇区大小
}__attribute__ ((packed));//对齐512字节
#endif