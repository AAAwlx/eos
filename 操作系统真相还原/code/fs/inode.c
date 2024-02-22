#include"inode.h"
#include"global.h"
#include"debug.h"
#include"ide.h"
#include"string.h"
#include"interrupt.h"
#include"thread.h"
#include "super_block.h"
#include"printk.h"
#include"fs.h"
#include"file.h"
struct inode_position
{
    bool two_sec;// inode是否跨扇区
    uint32_t sec_lba;// 所在扇区号
    uint32_t off_size;// 偏移量
};
//获取inode所在的扇区和扇区内的偏移量
static void inode_locate(struct partition* part, uint32_t inode_no, struct inode_position* inode_pos)
{
    printk("inode_locate\n");
    ASSERT(inode_no < 4096);  // 判断是否超出最大inode号
    uint32_t inode_table_lba = part->sb->inode_table_lba;//读出inode块的起始地址
    uint32_t inode_size = sizeof(struct inode);//算出inode结构体的大小
    uint32_t off_size = inode_size * inode_no;//在磁盘中的偏移量就是
    uint32_t off_sec = off_size / 512;//占据磁盘数
    uint32_t off_size_in_sec = off_sec % 512;//扇区内的偏移量
    uint32_t left_in_sec = 512 - off_size_in_sec;
    struct inode_position* i;
    if (left_in_sec < inode_size) {
        i->off_size = true;
    } else {
        i->off_size = false;
    }
    i->sec_lba = inode_table_lba + off_sec;
    i->two_sec = off_size_in_sec;
}
//将inode写入磁盘之中
void inode_sync(struct partition* part, struct inode* inode, void* io_buf)
{
    uint8_t inode_no = inode->i_no;
    struct inode_position inode_pos;
    inode_locate(part, inode_no, &inode_pos);
    ASSERT(inode_pos.sec_lba <= (part->sec_cnt + part->start_lba));//检查inode的位置是否在本分区之内
    struct inode pure_inode;
    memcpy(&pure_inode, inode,sizeof(struct inode));
    //清楚inode中的几项
    pure_inode.write_deny = false;
    pure_inode.i_open_cnts = 0;
    pure_inode.inode_tag.prev = NULL;//将inode的前继节点
    char* inode_buf = (char*)io_buf;
    if (inode_pos.two_sec)
    {
        ide_read(part->my_disk, io_buf, inode_pos.sec_lba, 2);//将其实的那个块读取出来
        memcpy((io_buf + inode_pos.off_size), &pure_inode, sizeof(struct inode));//拼凑上新写入的inode表项
        ide_write(part->my_disk, io_buf, inode_pos.sec_lba, 2);//将写入新表项之后的
    } else {
        ide_read(part->my_disk, io_buf, inode_pos.sec_lba, 1);
        memcpy((io_buf + inode_pos.off_size), &pure_inode, sizeof(struct inode));
        ide_write(part->my_disk, io_buf, inode_pos.sec_lba, 1);
    }
}
struct inode* inode_open(struct partition* part, uint32_t inode_no) {
    
    // 先在已经打开的inode链表中找inode.此链表相当于缓冲哦
    struct list_node* elem = part->open_inodes.head.next;
    struct inode* inode_found;
    while (elem != &part->open_inodes.tail) {
        inode_found = elem2entry(struct inode, inode_tag, elem);
        if (inode_found->i_no == inode_no) {
            inode_found->i_open_cnts++;
            return inode_found;
        }
        elem = elem->next;
  }
  /*从缓冲中没有找到*/
  struct inode_position inode_pos;
  inode_locate(part, inode_no, &inode_pos);  // 获取在磁盘中的位置
  struct task_pcb* cur = running_thread();

  /* 为使通过 sys_malloc 创建的新 inode 被所有任务共享, 需要将 inode
 置于内核空间,故需要临时 将 cur_pbc->pgdir 置为 NULL */
  uint32_t* cur_pagedir_bak = cur->pgdir;
  cur->pgdir = NULL;  // 暂时置为空
  inode_found = (struct inode*)sys_malloc(sizeof(struct inode));
  /*恢复pgdir*/
  cur->pgdir = cur_pagedir_bak;
  char* inode_buf;
  if (inode_pos.two_sec) {  // 跨扇区
    inode_buf = (char*)sys_malloc(1024);
    ide_read(part->my_disk, inode_buf,inode_pos.sec_lba,  2);
  } else {
    inode_buf = (char*)sys_malloc(512);
    ide_read(part->my_disk,inode_buf, inode_pos.sec_lba,  1);
  }
  memcpy(inode_found, inode_buf + inode_pos.off_size, sizeof(struct inode));

  // 插入队头，因为最可能被访问到
  list_push(&part->open_inodes, &inode_found->inode_tag);
  inode_found->i_open_cnts = 1;
  sys_free(inode_buf);
  return inode_found;
}
void inode_close(struct inode* inode) {
   /* 若没有进程再打开此文件,将此inode去掉并释放空间 */
   enum intr_status old = intr_disable();
   if (--inode->i_open_cnts == 0) {
      list_remove(&inode->inode_tag);	  // 将I结点从part->open_inodes中去掉
   /* inode_open时为实现inode被所有进程共享,
    * 已经在sys_malloc为inode分配了内核空间,
    * 释放inode时也要确保释放的是内核内存池 */
      struct task_pcb* cur = running_thread();
      uint32_t* cur_pagedir_bak = cur->pgdir;
      cur->pgdir = NULL;
      sys_free(inode);
      cur->pgdir = cur_pagedir_bak;
   }
   intr_set_status(old);
}
//将inode写入到part之中
void inode_init(uint32_t inode_no,struct inode* new_i)//初始化inode结构体
{
    new_i->i_no = inode_no;  // inode号
    new_i->i_open_cnts = 0;
    new_i->i_size = 0;
    new_i->write_deny = false;
    for (uint8_t i = 0; i < 13; i++) {
        new_i->i_sectors[i] = 0;
    }
}
void inode_delete(struct partition* part, uint32_t inode_no, void* io_buf)
{
    ASSERT(inode_no < 4096);//判断inode是否合法
    struct inode_position inode_pos;
    inode_locate(cur_part, inode_no, &inode_pos);
    ASSERT(inode_pos.sec_lba <= (part->start_lba + part->sec_cnt));//判断获取的inode表项位置是否合法
    char* buf = io_buf;
    if (inode_pos.two_sec)  // 是否跨越扇区
    {
        ide_read(cur_part->my_disk, buf, inode_pos.sec_lba, 2);
        memset((buf + inode_pos.off_size), 0, 2);
        ide_write(cur_part->my_disk, buf, inode_pos.sec_lba, 2);
    } else {
        ide_read(cur_part->my_disk, buf, inode_pos.sec_lba, 1);
        memset((buf + inode_pos.off_size), 0, 1);
        ide_write(cur_part->my_disk, buf, inode_pos.sec_lba, 1);
    }
}
void inode_release(struct partition* part, uint32_t inode_no)
{
    struct inode* inode_to_del = inode_open(cur_part, inode_no);
    ASSERT(inode_to_del->i_no == inode_no);
    uint8_t block_idx = 0,block_cnt=12;
    uint32_t block_bitmap_idx;
    uint32_t all_blocks[140] = {0};	  //12个直接块+128个间接块
    while (block_idx<12)
    {
        all_blocks[block_idx] = inode_to_del->i_sectors[block_idx];
        block_idx++;
    }
    if (inode_to_del->i_sectors[12]!=0)
    {
        ide_read(part->my_disk, all_blocks + 12, inode_to_del->i_sectors[12], 1);
        block_cnt = 140;
        block_bitmap_idx =
            inode_to_del->i_sectors[12] - part->sb->data_start_lba;
        ASSERT(block_bitmap_idx > 0);
        bitmap_set(&part->block_bitmap, block_bitmap_idx, 0);
        bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
    }
    block_idx = 0;
    while (block_idx<block_cnt)
    {
        if (all_blocks[block_idx]!=0)
        {
            block_bitmap_idx = 0;
            block_bitmap_idx=all_blocks[block_idx]- part->sb->data_start_lba;
            ASSERT(block_bitmap_idx > 0);
	        bitmap_set(&part->block_bitmap, block_bitmap_idx, 0);
	        bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
        }
        block_idx++;
    }
    bitmap_set(&part->inode_bitmap, inode_no, 0);  
    bitmap_sync(cur_part, inode_no, INODE_BITMAP);
    void* io_buf = sys_malloc(1024);
    inode_delete(part, inode_no, io_buf);
    sys_free(io_buf);
    inode_close(inode_to_del);
}