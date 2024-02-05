#include "dir.h"
#include "stdint.h"
#include "inode.h"
#include "file.h"
#include "fs.h"
#include "printk.h"
#include "global.h"
#include "debug.h"
#include "memory.h"
#include "string.h"
#include "interrupt.h"
#include "super_block.h"

struct dir root_dir;             // 根目录
void open_root_dir(struct partition* part)
{
    root_dir.inode = inode_open(part, part->sb->root_inode_no);
    root_dir.dir_pos = 0;
}
struct dir* dir_open(struct partition* part, uint32_t inode_no)
{
    struct dir* d =(struct dir*)sys_malloc(sizeof(struct dir));
    d->inode = inode_open(part, inode_no);
    d->dir_pos = 0;
    return;
}
void dir_close(struct dir* dir)
{
    if (dir==&root_dir)
    {
        return;
    }
    inode_close(dir->inode);
    sys_free(dir);
}
/* 在part分区内的pdir目录内寻找名为name的文件或目录,
 * 找到后返回true并将其目录项存入dir_e,否则返回false */
bool search_dir_entry(struct partition* part, struct dir* pdir, const char* name, struct dir_entry* dir_e)
{
    uint32_t block_cnt = 140;//12个直接块加上128个间接块
    uint32_t* all_blocks = (uint32_t*)sys_malloc(48 + 512);//为文件块申请缓冲区
    if (all_blocks == NULL) {
        printk("search_dir_entry: sys_malloc for all_blocks failed");
        return false;
    }
    uint32_t block_idx = 0;
    while (block_idx<12)
    {
        all_blocks[block_idx] = pdir->inode->i_sectors[block_idx];
        block_idx++;
    }
    block_idx = 0;
    if (all_blocks[12]!=0)
    {
        ide_read(part->my_disk, all_blocks + 12, pdir->inode->i_sectors[12],1);
    }
    uint8_t* buf = (uint8_t*)sys_malloc(SECTOR_SIZE);
    struct dir_entry* p_de = (struct dir_entry*)buf;
    uint32_t dir_entry_size = part->sb->dir_entry_size;//目录项的大小
    uint32_t dir_entry_cnt = SECTOR_SIZE / dir_entry_size;//一个扇区中
    while (block_idx<block_cnt)
    {
        if (all_blocks[block_idx]==0)//如果该指针中没有内容则向下一条继续寻找
        {
            block_idx++;
            continue;
        }
        ide_read(part->my_disk, buf, all_blocks[block_idx], 1);
        uint32_t dir_entry_idx = 0;
        while (dir_entry_idx<dir_entry_cnt)
        {
            if (!strcmp(p_de->filename,name))//如果找到了整个目录项
            {
                memcpy(dir_e, p_de, dir_entry_size);
                sys_free(buf);
	            sys_free(all_blocks);
	            return true;
            }
            dir_entry_idx++;
            p_de++;
        }
        block_idx++;
        p_de=(struct dir_entry*)buf;
        memset(buf, 0, SECTOR_SIZE);
    }
    sys_free(buf);
    sys_free(all_blocks);
    return false;
}
void create_dir_entry(char* filename, uint32_t inode_no, uint8_t file_type, struct dir_entry* p_de)
{
    ASSERT(strlen(filename) <= MAX_FILE_NAME_LEN);
    p_de->f_type = file_type;
    p_de->i_no = inode_no;
    memcpy(p_de->filename, filename, strlen(filename));
}
bool sync_dir_entry(struct dir* parent_dir, struct dir_entry* p_de, void* io_buf)
{
    struct inode* dir_inode = parent_dir->inode;
    uint32_t dir_size = dir_inode->i_size;//目录中的总和
    uint32_t dir_entry_size = cur_part->sb->dir_entry_size;//单个目录项的大小
    ASSERT(dir_size % dir_entry_size == 0);
    uint32_t dir_entry_per_sec = (512 / dir_size);//一个扇区中可容纳的目录项
    uint32_t block_lba = -1;
    uint8_t block_idx = 0;
    uint32_t all_blocks[140] = {0};
    while (block_idx<12)
    {
        all_blocks[block_idx] = dir_inode->i_sectors[block_idx];
        block_idx++;
    }
    struct dir_entry* dir_e = (struct dir_entry*)io_buf;//目录项接收缓冲区
    int32_t block_bitmap_idx = -1;
    block_idx = 0;
    while (block_idx<140)//12个一级块加上128个间接块
    {
        block_bitmap_idx = -1;
        if (all_blocks[block_idx]==0)//如果该块指针尚未被分配指向块区域
        {
            block_lba = block_bitmap_alloc(cur_part);//位图中的第多少位,也就是在整个磁盘上的扇区号
            if (block_lba==-1)
            {
                printk("alloc block bitmap for sync_dir_entry failed\n");
                return false;
            }
            block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;//磁盘块之间的差距
            ASSERT(block_bitmap_idx != -1);
            bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
            block_bitmap_idx = -1;
            if (block_idx<12)//如果是直接块
            {
                dir_inode->i_sectors[block_idx] = all_blocks[block_idx];
            }else if (block_idx==12)//尚未分配块12
            {
                dir_inode->i_sectors[12] = block_lba;//将为12间接块分配的磁盘号写入到inode指针中
                block_lba = -1;
                block_lba = block_bitmap_alloc(cur_part);
                if (block_lba==-1)//资源回滚，释放之前分配的
                {
                    block_bitmap_idx =dir_inode->i_sectors[12] - cur_part->sb->data_start_lba;
                    bitmap_set(&cur_part->block_bitmap, block_bitmap_idx, 0);
                    bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);  // 重新同步
                    dir_inode->i_sectors[12] = 0;
                    printk("alloc block bitmap for sync_dir_entry failed\n");
                    return false;
                }
                block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
                ASSERT(block_bitmap_idx != -1);//判断在位图中的位置是否合法
                bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
                all_blocks[12] = block_lba;//将二级指针的位置写入到
                ide_write(cur_part->my_disk, dir_inode->i_sectors[12], all_blocks + 12,1);
            } else  // 已经分配了块12
            {
                //将新分配的磁盘块位置写入到inode文件指针中
                all_blocks[block_idx] = block_lba;
                ide_write(cur_part->my_disk, all_blocks+12/*将自12起的所有内容都写入磁盘中*/, dir_inode->i_sectors[12], 1);
            }
            memset(io_buf, 0, 512);
            memcpy(io_buf, p_de, 512);
            ide_write(cur_part->my_disk, io_buf, block_lba, 1);//将目录项写入到新分配的磁盘块中
            dir_inode->i_size += dir_entry_size;
            return true;
        }
        ide_read(cur_part->my_disk, io_buf, all_blocks[block_idx], 1);
        uint8_t dir_idx;
        while (dir_idx)
        {
            if ((dir_e+dir_idx)->f_type==FT_UNKNOWN)//如果此目录还未被使用
            {
                memcpy(dir_e + dir_idx, p_de, sizeof(struct dir_entry));//如果是空目录项就将目录内容写入
                ide_write(cur_part->my_disk, dir_e, all_blocks[block_idx], 1);
                return true;
            }
            dir_idx++;
        }
        block_idx++;
        if (block_idx>12)
        {
            ide_read(cur_part->my_disk, all_blocks+12,dir_inode->i_sectors[12], 1);
        }
    }
    //找完所有文件块还是没有找到空目录项返回错误
    printk("directory is full!\n");
    return false;
}