#ifndef DEVICE_IDE
#define DEVICE_IDE
#include"stdint.h"
#include "list.h"
#include"../kernel/memory.h"
#include "sysnc.h"
struct partition {
   uint32_t start_lba;		 // 起始扇区
   uint32_t sec_cnt;		 // 扇区数
   struct disk* my_disk;	 // 分区所属的硬盘
   struct list_node part_tag;	 // 用于队列中的标记
   char name[8];		 // 分区名称
   struct super_block* sb;	 // 本分区的超级块
   struct bitmap block_bitmap;	 // 块位图
   struct bitmap inode_bitmap;	 // i结点位图
   struct list open_inodes;	 // 本分区打开的i结点队列
};
struct disk {
    char name[8];
    struct ide_channel* mychannel;//磁盘的通道
    uint8_t dev_no;//标记是主盘还是从盘
    struct partition mian_parts[4];//四个主分区
    struct partition logic_parts[4];//八个逻辑分区

};
struct ide_channel {
    char name[8];
    uint32_t port_base;//起始端口号
    uint8_t inr_no;//中断号
    struct lock* c_lock;//通道锁，一个通道上有两个磁盘
    bool expect_intr;//是否等待中断
    struct semaphore disk_down;
    struct disk devices[2];//两个磁盘
};
void ide_init();
#endif