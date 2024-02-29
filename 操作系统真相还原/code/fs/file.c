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
struct file file_table[MAX_FILE_OPEN];

/* 从文件表file_table中获取一个空闲位,成功返回下标,失败返回-1 */
int32_t get_free_slot_in_global(void)
{
    uint32_t fd_idx = 3;
    while (fd_idx < MAX_FILE_OPEN)
    {
        if (file_table[fd_idx].fd_inode == NULL)
        {
            break;
        }
        fd_idx++;
    }
    if (fd_idx == MAX_FILE_OPEN)
    {
        printk("exceed max open files\n");
        return -1;
    }
    return fd_idx;
}

/* 将全局描述符下标安装到进程或线程自己的文件描述符数组fd_table中,
 * 成功返回下标,失败返回-1 */
int32_t pcb_fd_install(int32_t globa_fd_idx)
{
    struct task_pcb *cur = running_thread();
    uint8_t local_fd_idx = 3; // 跨过stdin,stdout,stderr
    while (local_fd_idx < MAX_FILES_OPEN_PER_PROC)
    {
        if (cur->fd_table[local_fd_idx] == -1)
        { // -1表示free_slot,可用
            cur->fd_table[local_fd_idx] = globa_fd_idx;
            break;
        }
        local_fd_idx++;
    }
    if (local_fd_idx == MAX_FILES_OPEN_PER_PROC)
    {
        printk("exceed max open files_per_proc\n");
        return -1;
    }
    return local_fd_idx;
}

/* 分配一个i结点,返回i结点号 */
int32_t inode_bitmap_alloc(struct partition *part)
{
    int32_t bit_idx = bitmap_scan(&part->inode_bitmap, 1);
    if (bit_idx == -1)
    {
        return -1;
    }
    bitmap_set(&part->inode_bitmap, bit_idx, 1);
    return bit_idx;
}

/* 分配1个扇区,返回其扇区地址 */
int32_t block_bitmap_alloc(struct partition *part)
{
    int32_t bit_idx = bitmap_scan(&part->block_bitmap, 1);
    if (bit_idx == -1)
    {
        return -1;
    }
    bitmap_set(&part->block_bitmap, bit_idx, 1);
    /* 和inode_bitmap_malloc不同,此处返回的不是位图索引,而是具体可用的扇区地址 */
    return (part->sb->data_start_lba + bit_idx);
}

/* 将内存中bitmap第bit_idx位所在的512字节同步到硬盘 */
void bitmap_sync(struct partition *part, uint32_t bit_idx, uint8_t btmp_type)
{
    uint32_t off_sec = bit_idx / 4096;        // 本i结点索引相对于位图的扇区偏移量
    uint32_t off_size = off_sec * BLOCK_SIZE; // 本i结点索引相对于位图的字节偏移量
    uint32_t sec_lba;
    uint8_t *bitmap_off;

    /* 需要被同步到硬盘的位图只有inode_bitmap和block_bitmap */
    switch (btmp_type)
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
    ide_write(part->my_disk,  bitmap_off, sec_lba,1);
}

/* 创建文件,若成功则返回文件描述符,否则返回-1 */
int32_t file_create(struct dir *parent_dir, char *filename, uint8_t flag)
{
    /* 后续操作的公共缓冲区 */
    void *io_buf = sys_malloc(1024);
    if (io_buf == NULL)
    {
        printk("in file_creat: sys_malloc for io_buf failed\n");
        return -1;
    }

    uint8_t rollback_step = 0; // 用于操作失败时回滚各资源状态

    /* 为新文件分配inode */
    int32_t inode_no = inode_bitmap_alloc(cur_part);
    if (inode_no == -1)
    {
        printk("in file_creat: allocate inode failed\n");
        goto rollback;
    }
    
    /* 此inode要从堆中申请内存,不可生成局部变量(函数退出时会释放)
     * 因为file_table数组中的文件描述符的inode指针要指向它.*/
    struct inode *new_file_inode = (struct inode *)sys_malloc(sizeof(struct inode));
    if (new_file_inode == NULL)
    {
        printk("file_create: sys_malloc for inode failded\n");
        rollback_step = 1;
        goto rollback;
    }
    inode_init(inode_no, new_file_inode); // 初始化i结点

    /* 返回的是file_table数组的下标 */
    int fd_idx = get_free_slot_in_global();
    if (fd_idx == -1)
    {
        printk("exceed max open files\n");
        rollback_step = 2;
        goto rollback;
    }

    file_table[fd_idx].fd_inode = new_file_inode;
    file_table[fd_idx].fd_pos = 0;
    file_table[fd_idx].fd_flag = flag;
    file_table[fd_idx].fd_inode->write_deny = false;

    struct dir_entry new_dir_entry;
    memset(&new_dir_entry, 0, sizeof(struct dir_entry));

    create_dir_entry(filename, inode_no, FT_REGULAR, &new_dir_entry); // create_dir_entry只是内存操作不出意外,不会返回失败

    /* 同步内存数据到硬盘 */
    /* a 在目录parent_dir下安装目录项new_dir_entry, 写入硬盘后返回true,否则false */
    if (!sync_dir_entry(parent_dir, &new_dir_entry, io_buf))
    {
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

    /* d 将inode_bitmap位图同步到硬盘 */
    bitmap_sync(cur_part, inode_no, INODE_BITMAP);

    /* e 将创建的文件i结点添加到open_inodes链表 */
    list_push(&cur_part->open_inodes, &new_file_inode->inode_tag);
    new_file_inode->i_open_cnts = 1;

    sys_free(io_buf);
    return pcb_fd_install(fd_idx);

/*创建文件需要创建相关的多个资源,若某步失败则会执行到下面的回滚步骤 */
rollback:
    switch (rollback_step)
    {
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
int32_t file_write(struct file* file, const void* buf, uint32_t count)
{
    if (file->fd_inode->i_size+count>(BLOCK_SIZE*140))//判断文件大小是否合理，文中12个直接块128个间接块，只能支持最大140个块的文件
    {
        printk("exceed max file_size 71680 bytes, write file failed\n");
        return -1;
    }
    uint8_t* io_buf = sys_malloc(BLOCK_SIZE);
    if (io_buf==NULL)
    {
        printk("file_write: sys_malloc for io_buf failed\n");
        return -1;
    }
    uint32_t* all_block = sys_malloc(BLOCK_SIZE+48);//记录文件块地址
    if (all_block==NULL)
    {
        printk("file_write: sys_malloc for all_blocks failed\n");
        return -1;
    }
    //为写入的文件分配块
    int32_t block_lba = -1; //块地址
    int32_t block_bitmap_idx = 0;//块位图索引
    int32_t indirect_block_table;  // 用来获取一级间接表地址
    uint32_t block_idx;            // 块索引 
    if (file->fd_inode->i_sectors[0] == 0) {//如果文件被创建出来第一次写入
        block_lba = block_bitmap_alloc(cur_part);//分配一个块
        if (block_lba==-1)
        {
            printk("file_write: block_bitmap_alloc failed\n");
            return -1;
        }
        file->fd_inode->i_sectors[0] == block_lba;
        block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
        ASSERT(block_bitmap_idx != 0);
        bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);//更新块位图
    }
    uint32_t file_has_used_blocks = file->fd_inode->i_size / BLOCK_SIZE + 1;//之前使用过得块数量
    uint32_t file_will_use_blocks =
        (file->fd_inode->i_size + count) / BLOCK_SIZE + 1;//写入新内容后文件需要使用的块数量
    ASSERT(file_will_use_blocks <= 140);
    uint32_t add_block = file_will_use_blocks - file_has_used_blocks;
    if (add_block==0) {//如果不需要新增块
        if (file_has_used_blocks<=12)//如果在直接块范围内
        {
            block_idx = file_has_used_blocks - 1;
            all_block[block_idx] = file->fd_inode->i_sectors[block_idx];
        } else  // 如果是间接块
        {
            ASSERT(file->fd_inode->i_sectors[12] != 0);
            indirect_block_table=file->fd_inode->i_sectors[12];  // 用来获取一级间接表地址
            ide_read(cur_part->my_disk, all_block+12, file->fd_inode->i_sectors[12],
                     1);//将间接块读入到all第12位之后
        }
    }else//需要新增块
    {
        if (file_will_use_blocks<=12)//写入后仍然只使用直接块
        {
            block_idx = file_has_used_blocks - 1;
            ASSERT(file->fd_inode->i_sectors[block_idx] == 0);
            all_block[block_idx] = file->fd_inode->i_sectors[block_idx];
            block_idx++;
            while (block_idx < file_will_use_blocks) {
                block_lba = block_bitmap_alloc(cur_part);
                if (block_lba==-1)
                {
                    printk("file_write: block_bitmap_alloc for situation 1 failed\n");
                    return -1;
                }
                ASSERT(file->fd_inode->i_sectors[block_idx] ==
                       0);  // 确保尚未分配扇区地址
                all_block[block_idx] = block_lba;
                file->fd_inode->i_sectors[block_idx]= block_lba;
                block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
                ASSERT(block_bitmap_idx != 0);
                bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);//更新块位图
                block_idx++;
            }

        } else if (file_will_use_blocks > 12 && file_has_used_blocks <= 12)  // 写入前未使用间接块，写入后需要使用
        {
            //将可能未写满的块备份入all_block数组中
            block_idx = file_has_used_blocks - 1;
            all_block[block_idx] = file->fd_inode->i_sectors[block_idx];
            //为间接块分配地址
            block_lba = block_bitmap_alloc(cur_part);
            if (block_lba==-1)
            {
                printk("file_write: block_bitmap_alloc for situation 2 failed\n");
                return -1;
            }
            ASSERT(file->fd_inode->i_sectors[12] ==
                       0);  // 确保尚未分配扇区地址
            indirect_block_table = block_lba;//记录索引块的位置
            file->fd_inode->i_sectors[12]= block_lba;
            block_bitmap_idx = block_lba - cur_part->sb->block_bitmap_lba;
            bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
            block_idx = file_has_used_blocks;
            while (block_idx<file_will_use_blocks)
            {
                block_lba = block_bitmap_alloc(cur_part);//申请一个新块
                 if (block_lba == -1) {
                    printk("file_write: block_bitmap_alloc for situation 2 failed\n");
                    return -1;
                }
                if(block_idx<12)//如果使用的是直接块
                {
                    ASSERT(file->fd_inode->i_sectors[block_idx] ==0);  // 确保尚未分配扇区地址
                    all_block[block_idx] = block_lba;
                    file->fd_inode->i_sectors[block_idx]= block_lba;
                    
                }else
                {
                    all_block[block_idx] = block_lba;
                }
                block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
                ASSERT(block_bitmap_idx != 0);
                bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);//更新块位图
                block_idx++;
            }
            ide_write(cur_part->my_disk, all_block + 12,file->fd_inode->i_sectors[12], 1);
        } else  if(file_has_used_blocks>12)// 写入前就已经使用间接块
        {
            ASSERT(file->fd_inode->i_sectors[12] != 0);
            indirect_block_table = file->fd_inode->i_sectors[12];
            ide_read(cur_part->my_disk, all_block + 12, indirect_block_table,1);
            block_idx = file_has_used_blocks;
            while (block_idx<file_will_use_blocks)
            {
                block_lba = block_bitmap_alloc(cur_part);
                if (block_lba == -1) {
                    printk("file_write: block_bitmap_alloc for situation 3 failed\n");
                    return -1;
                }
                all_block[block_idx++] = block_idx;
                block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
                bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
            }
            ide_write(cur_part, all_block + 12, indirect_block_table, 1);
        }
    }
    //向块中写入内容
    const uint8_t* src = buf;    // 用src指向buf中待写入的数据
    uint32_t bytes_written = 0;  // 用来记录已写入数据大小
    uint32_t size_left = count;  // 用来记录未写入数据大小
    uint32_t sec_idx;              // 用来索引扇区
    uint32_t sec_lba;              // 扇区地址
    uint32_t sec_off_bytes;        // 扇区内字节偏移量
    uint32_t sec_left_bytes;       // 扇区内剩余字节量
    uint32_t chunk_size;           // 每次写入硬盘的数据块大小
    bool first_write_block = true;  // 含有剩余空间的块标识
    file->fd_pos = file->fd_inode->i_size - 1;//设置文件指针的偏移量
    while (bytes_written<count)
    {
        sec_idx = file->fd_inode->i_size / BLOCK_SIZE;
        sec_lba = all_block[sec_idx];
        sec_off_bytes = file->fd_inode->i_size % BLOCK_SIZE;
        sec_left_bytes = BLOCK_SIZE - sec_off_bytes;
        chunk_size = size_left < sec_left_bytes ? size_left : sec_left_bytes;//如果本次写入的数据量小于磁盘中剩余的量
        if (first_write_block) {
            ide_read(cur_part->my_disk, io_buf, sec_lba, 1);
            first_write_block = false;
        }
        memcpy(io_buf + sec_off_bytes, src, chunk_size);
        ide_write(cur_part->my_disk, io_buf, sec_lba, 1);
        printk("file write at lba 0x%x\n", sec_lba);  // 调试,完成后去掉
        src += chunk_size;//移动待输入字符串的指针
        file->fd_inode->i_size += chunk_size;  // 更新文件大小
        file->fd_pos += chunk_size;//文件指针偏移
        bytes_written += chunk_size;//已写入字节数
        size_left -= chunk_size;
    }

    //更新inode
    inode_sync(cur_part, file->fd_inode, io_buf);
    //释放资源
    sys_free(all_block);
    sys_free(io_buf);
    return bytes_written;
}
int32_t file_read(struct file* file, void* buf, uint32_t count) 
{
    uint8_t* buf_dst = (uint8_t*)buf;
    uint32_t size = count;
    uint32_t size_left = size;
    if ((file->fd_pos+count)>file->fd_inode->i_size)//如果要读取的量大于文件剩余的数量
    {
        size = file->fd_inode->i_size - file->fd_pos;//文件当前的
        size_left = size;
        if (size==0)
        {
            return -1;
        }
    }
    uint8_t* io_buf = sys_malloc(BLOCK_SIZE);
    if (io_buf==NULL)
    {
        printk("file_read: sys_malloc for io_buf failed\n");
        return -1;
    }
    uint32_t* all_blocks = sys_malloc(BLOCK_SIZE + 48);
    if (all_blocks==NULL)
    {
        printk("file_read: sys_malloc for all_blocks failed\n");
        return -1;
    }
    uint32_t block_read_start_idx = file->fd_pos / BLOCK_SIZE;
    uint32_t block_read_end_idx = (file->fd_pos + size) / BLOCK_SIZE;
    ASSERT(block_read_start_idx < 139 && block_read_end_idx < 139);//判断开始与结束是否符合规范
    uint32_t read_blocks = block_read_end_idx - block_read_start_idx;
    int32_t indirect_block_table;  // 用来获取一级间接表地址
    uint32_t block_idx;            // 获取待读取的块地址
    if (read_blocks==0)//读一个块中的内容
    {
        if (block_read_end_idx<12)//直接块
        {
            block_idx = block_read_end_idx;
            all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];
        } else {
            indirect_block_table = file->fd_inode->i_sectors[12];
            ide_read(cur_part->my_disk, all_blocks + 12, indirect_block_table,1);
        }
    }else //读多个块
    {
        if (block_read_end_idx<12)//只有直接块
        {
            block_idx = block_read_start_idx;
            while (block_idx<block_read_end_idx)
            {
                all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];
                block_idx++;
            }
        } else if (block_read_start_idx < 12 && block_read_end_idx >= 12)  // 间接块直接块都有
        {
            block_idx = block_read_start_idx;
            while (block_idx<12)
            {
                all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];
                block_idx++;
            }
            ASSERT(file->fd_inode->i_sectors[12] != 0);
            indirect_block_table = file->fd_inode->i_sectors[12];
            ide_read(cur_part->my_disk, all_blocks + 12, indirect_block_table,1);
        } else  // 只有间接块
        {
            ASSERT(file->fd_inode->i_sectors[12] != 0);
            indirect_block_table = file->fd_inode->i_sectors[12];
            ide_read(cur_part->my_disk, all_blocks + 12, indirect_block_table,1);
        }
    }
    uint32_t sec_idx, sec_lba, sec_off_bytes, sec_left_bytes, chunk_size;
    uint32_t bytes_read = 0;
    while (bytes_read<size)
    {
        sec_idx = file->fd_pos / BLOCK_SIZE;
        sec_lba = all_blocks[sec_idx];
        sec_off_bytes = file->fd_pos % BLOCK_SIZE;
        sec_left_bytes = BLOCK_SIZE - sec_off_bytes;
        chunk_size = size_left < sec_left_bytes ? size_left : sec_left_bytes;
        memset(io_buf, 0, BLOCK_SIZE);
        ide_read(cur_part->my_disk, io_buf, sec_lba, 1);
        memcpy(buf_dst, io_buf + sec_off_bytes, chunk_size);
        buf_dst += chunk_size;
        file->fd_pos += chunk_size;
        bytes_read += chunk_size;
        size_left -= chunk_size;
    }
    sys_free(io_buf);
    sys_free(all_blocks);
    return bytes_read;
}