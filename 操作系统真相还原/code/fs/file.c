#include "file.h"
#include "fs.h"
#include "super_block.h"
#include "inode.h"
#include "printk.h"
#include "memory.h"
#include "debug.h"
#include "interrupt.h"
#include "string.h"
#include "thread.h"
#include "global.h"
#include"ide.h"
#define DEFAULT_SECS 1

/* 文件表 */
struct file file_table[MAX_FILE_OPEN];
//找到文件表中的
int32_t get_free_slot_in_global(void)
{
    uint8_t i;
    for (i = 0; i < MAX_FILE_OPEN; i++) {
        if (file_table[i].fd_inode == NULL)
        {
            return i;
        }
    }
    if (i==MAX_FILE_OPEN)
    {
        printk("exceed max open files\n");
         return -1;
    }
}
/* 将全局描述符下标安装到进程或线程自己的文件描述符数组fd_table中,
 * 成功返回下标,失败返回-1 */
int32_t pcb_fd_install(int32_t globa_fd_idx)
{
    struct task_pcb* thread = running_thread();
    uint8_t index=3;
    while (index) {
        if (thread->fd_table[index]==-1)
        {
            thread->fd_table[index] = globa_fd_idx;
            break;
        }
        index++;
    }
    if (index==MAX_FILES_OPEN_PER_PROC)
    {
        printk("exceed max open files_per_proc\n");
      return -1;
    }
    return index;//返回文件描述符号
}
int32_t inode_bitmap_alloc(struct partition* part)
{
    int32_t bit_idx = bitmap_scan(&part->inode_bitmap, 1);
   if (bit_idx == -1) {
      return -1;
   }
   bitmap_set(&part->inode_bitmap, bit_idx, 1);
   return bit_idx;
}
int32_t block_bitmap_alloc(struct partition* part)
{
    int32_t bit_idx = bitmap_scan(&part->block_bitmap, 1);
   if (bit_idx == -1) {
      return -1;
   }
   bitmap_set(&part->block_bitmap, bit_idx, 1);
   return (part->sb->data_start_lba + bit_idx);
}
void bitmap_sync(struct partition* part, uint32_t bit_idx, uint8_t btmp)
{
    uint32_t off_sec = bit_idx / 4096;//在扇区中的偏移量
    uint32_t off_size = off_sec * BLOCK_SIZE;//在位图中字节的偏移量
    uint32_t sec_lba;
    uint8_t* bitmap_off;
    switch (btmp)
    {
    case INODE_BITMAP:
        sec_lba = part->sb->inode_bitmap_lba + off_sec;
        bitmap_off = part->inode_bitmap.bits + off_size;
        break;
    case BLOCK_BITMAP:
        sec_lba = part->sb->block_bitmap_lba + off_sec;
        bitmap_off = part->block_bitmap.bits + off_size;
        break;
    }
    ide_write(part->my_disk, bitmap_off, sec_lba, 1);
}