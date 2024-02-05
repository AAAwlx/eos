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
   return (part->sb->data_start_lba + bit_idx);//位图中的位再加上数据区起始的位置，就是空闲块所在的磁盘扇区号
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
int32_t file_create(struct dir* parent_dir, char* filename, uint8_t flag)
{
    char* io_buf = (char*)sys_malloc(1024);
    if (io_buf==NULL)
    { 
        printk("in file_creat: sys_malloc for io_buf failed\n");
        return -1;
    }
    uint8_t rollback_step = 0;	       // 用于操作失败时回滚各资源状态
    //分配inode号
    uint8_t inode_no = inode_bitmap_alloc(cur_part);
    if (inode_no==-1)
    {
        printk("in file_creat: allocate inode failed\n");
        return -1;
    }
    //初始化inode结构体
    struct inode* new_file_inode = (struct inode*)sys_malloc(sizeof(struct inode)); 
    if (new_file_inode == NULL) {
        printk("file_create: sys_malloc for inode failded\n");
        rollback_step = 1;
        goto rollback;
    }
    inode_init(inode_no, new_file_inode);
    //在系统支持的打开文件描述符中为新建文件分配
    int fd_idx = get_free_slot_in_global();
    if (fd_idx==-1)
    {
        printk("exceed max open files\n");
        rollback_step = 2;
        goto rollback;
    }
    file_table[fd_idx].fd_flag = flag;
    file_table[fd_idx].fd_inode = new_file_inode;
    file_table[fd_idx].fd_pos = 0;
    file_table[fd_idx].fd_inode->write_deny = false;
    struct dir_entry new_dir_entry;
    memset(&new_dir_entry, 0, sizeof(struct dir_entry));
    create_dir_entry(filename, inode_no, FT_DIRECTORY, &new_dir_entry);
    if (sync_dir_entry(parent_dir, &new_dir_entry, io_buf)) {
        printk("sync dir_entry to disk failed\n");
        rollback_step = 3;
        goto rollback;
    }
    memset(io_buf, 0, 1024);
    /* b 将父目录i结点的内容同步到硬盘 */
    inode_sync(cur_part, parent_dir->inode, io_buf);
    memset(io_buf, 0, 1024);
    /* c 将新创建文件的i结点内容同步到硬盘 */
    inode_sync(cur_part, new_file_inode, io_buf);
    //将inode位图写入磁盘
    bitmap_sync(cur_part, inode_no, INODE_BITMAP);
    list_push(&cur_part->open_inodes, &new_file_inode->inode_tag);
    new_file_inode->i_open_cnts = 1;

   sys_free(io_buf);
   return pcb_fd_install(fd_idx);
rollback:
    switch (rollback_step) {
        case 3:
            /* 失败时,将file_table中的相应位清空 */
            memset(&file_table[fd_idx], 0, sizeof(struct file));
        case 2:
            sys_free(new_file_inode);
        case 1:
            /* 如果新文件的i结点创建失败,之前位图中分配的inode_no也要恢复 */
            bitmap_set(&cur_part->inode_bitmap, inode_no, 0);
            break;
    }
    sys_free(io_buf);
    return -1;
}
int32_t file_open(uint32_t inode_no, uint8_t flag)
{
    int fd_idx = get_free_slot_in_global();
    if (fd_idx == -1) {
      printk("exceed max open files\n");
      return -1;
   }
   file_table[fd_idx].fd_inode = inode_open(cur_part, inode_no);
   file_table[fd_idx].fd_flag = flag;
   file_table[fd_idx].fd_pos = 0;
   bool* write_deny = &file_table[fd_idx].fd_inode->write_deny;
    if (flag & O_RDWR || flag & O_WRONLY)
    {
        enum intr_status old = intr_disable();
        if (!(*write_deny))//如果没有其他进程在写
        {
            *write_deny = true;
            intr_set_status(old);
        }else//如果有其他进程在写则返回
        {
            intr_set_status(old);
            printk("file can`t be write now, try again later\n");
	        return -1;
        }
    }
   return pcb_fd_install(fd_idx);
}
int32_t file_close(struct file* file) 
{
    if (file==NULL)
    {
        return -1;
    }
    file->fd_inode->write_deny = false;
    inode_close(file->fd_inode);
    file->fd_inode = NULL;
    return 0;
}