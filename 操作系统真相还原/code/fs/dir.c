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
struct dir root_dir; // 根目录

/* 打开根目录 */
void open_root_dir(struct partition *part)
{
    root_dir.inode = inode_open(part, part->sb->root_inode_no);
    root_dir.dir_pos = 0;
}

/* 在分区part上打开i结点为inode_no的目录并返回目录指针 */
struct dir *dir_open(struct partition *part, uint32_t inode_no)
{
    struct dir *pdir = (struct dir *)sys_malloc(sizeof(struct dir));
    pdir->inode = inode_open(part, inode_no);
    pdir->dir_pos = 0;
    return pdir;
}

/* 在part分区内的pdir目录内寻找名为name的文件或目录,
 * 找到后返回true并将其目录项存入dir_e,否则返回false */
bool search_dir_entry(struct partition *part, struct dir *pdir, const char *name, struct dir_entry *dir_e)
{
    uint32_t block_cnt = 140; // 12个直接块+128个一级间接块=140块

    /* 12个直接块大小+128个间接块,共560字节 */
    uint32_t *all_blocks = (uint32_t *)sys_malloc(48 + 512);
    if (all_blocks == NULL)
    {
        printk("search_dir_entry: sys_malloc for all_blocks failed");
        return false;
    }

    uint32_t block_idx = 0;
    while (block_idx < 12)
    {
        all_blocks[block_idx] = pdir->inode->i_sectors[block_idx];
        block_idx++;
    }
    block_idx = 0;

    if (pdir->inode->i_sectors[12] != 0)
    { // 若含有一级间接块表
        ide_read(part->my_disk,  all_blocks + 12,pdir->inode->i_sectors[12], 1);
    }
    /* 至此,all_blocks存储的是该文件或目录的所有扇区地址 */

    /* 写目录项的时候已保证目录项不跨扇区,
     * 这样读目录项时容易处理, 只申请容纳1个扇区的内存 */
    uint8_t *buf = (uint8_t *)sys_malloc(SECTOR_SIZE);
    struct dir_entry *p_de = (struct dir_entry *)buf; // p_de为指向目录项的指针,值为buf起始地址
    uint32_t dir_entry_size = part->sb->dir_entry_size;
    uint32_t dir_entry_cnt = SECTOR_SIZE / dir_entry_size; // 1扇区内可容纳的目录项个数

    /* 开始在所有块中查找目录项 */
    while (block_idx < block_cnt)
    {
        /* 块地址为0时表示该块中无数据,继续在其它块中找 */
        if (all_blocks[block_idx] == 0)
        {
            block_idx++;
            continue;
        }
        ide_read(part->my_disk,  buf, all_blocks[block_idx],1);

        uint32_t dir_entry_idx = 0;
        /* 遍历扇区中所有目录项 */
        while (dir_entry_idx < dir_entry_cnt)
        {
            /* 若找到了,就直接复制整个目录项 */
            if (!strcmp(p_de->filename, name))
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
        p_de = (struct dir_entry *)buf; // 此时p_de已经指向扇区内最后一个完整目录项了,需要恢复p_de指向为buf
        memset(buf, 0, SECTOR_SIZE);    // 将buf清0,下次再用
    }
    sys_free(buf);
    sys_free(all_blocks);
    return false;
}

/* 关闭目录 */
void dir_close(struct dir *dir)
{
    /*************      根目录不能关闭     ***************
     *1 根目录自打开后就不应该关闭,否则还需要再次open_root_dir();
     *2 root_dir所在的内存是低端1M之内,并非在堆中,free会出问题 */
    if (dir == &root_dir)
    {
        /* 不做任何处理直接返回*/
        return;
    }
    inode_close(dir->inode);
    sys_free(dir);
}

/* 在内存中初始化目录项p_de */
void create_dir_entry(char *filename, uint32_t inode_no, uint8_t file_type, struct dir_entry *p_de)
{
    ASSERT(strlen(filename) <= MAX_FILE_NAME_LEN);

    /* 初始化目录项 */
    memcpy(p_de->filename, filename, strlen(filename));
    p_de->i_no = inode_no;
    p_de->f_type = file_type;
}

/* 将目录项p_de写入父目录parent_dir中,io_buf由主调函数提供 */
bool sync_dir_entry(struct dir *parent_dir, struct dir_entry *p_de, void *io_buf)
{
    struct inode *dir_inode = parent_dir->inode;
    uint32_t dir_size = dir_inode->i_size;
    uint32_t dir_entry_size = cur_part->sb->dir_entry_size;

    ASSERT(dir_size % dir_entry_size == 0); // dir_size应该是dir_entry_size的整数倍

    uint32_t dir_entrys_per_sec = (512 / dir_entry_size); // 每扇区最大的目录项数目
    int32_t block_lba = -1;

    /* 将该目录的所有扇区地址(12个直接块+ 128个间接块)存入all_blocks */
    uint8_t block_idx = 0;
    uint32_t all_blocks[140] = {0}; // all_blocks保存目录所有的块

   /* 将12个直接块存入all_blocks */
    while (block_idx < 12) {
        all_blocks[block_idx] = dir_inode->i_sectors[block_idx];
    	block_idx++;
    }
   	block_idx = 0;
   	if (dir_inode->i_sectors[12] != 0) {	// 若含有一级间接块表
      	ide_read(cur_part->my_disk,all_blocks + 12, dir_inode->i_sectors[12],  1);
   	}

    struct dir_entry *dir_e = (struct dir_entry *)io_buf; // dir_e用来在io_buf中遍历目录项
    int32_t block_bitmap_idx = -1;

    /* 开始遍历所有块以寻找目录项空位,若已有扇区中没有空闲位,
     * 在不超过文件大小的情况下申请新扇区来存储新目录项 */
    block_idx = 0;
    while (block_idx < 140)
    { // 文件(包括目录)最大支持12个直接块+128个间接块＝140个块
        block_bitmap_idx = -1;
        if (all_blocks[block_idx] == 0)
        { // 在三种情况下分配块
            block_lba = block_bitmap_alloc(cur_part);
            if (block_lba == -1)
            {
                printk("alloc block bitmap for sync_dir_entry failed\n");
                return false;
            }

            /* 每分配一个块就同步一次block_bitmap */
            block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
            ASSERT(block_bitmap_idx != -1);
            bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);

            block_bitmap_idx = -1;
            if (block_idx < 12)
            { // 若是直接块
                dir_inode->i_sectors[block_idx] = all_blocks[block_idx] = block_lba;
            }
            else if (block_idx == 12)
            {                                         // 若是尚未分配一级间接块表(block_idx等于12表示第0个间接块地址为0)
                dir_inode->i_sectors[12] = block_lba; // 将上面分配的块做为一级间接块表地址
                block_lba = -1;
                block_lba = block_bitmap_alloc(cur_part); // 再分配一个块做为第0个间接块
                if (block_lba == -1)
                {
                    block_bitmap_idx = dir_inode->i_sectors[12] - cur_part->sb->data_start_lba;
                    bitmap_set(&cur_part->block_bitmap, block_bitmap_idx, 0);
                    dir_inode->i_sectors[12] = 0;
                    printk("alloc block bitmap for sync_dir_entry failed\n");
                    return false;
                }

                /* 每分配一个块就同步一次block_bitmap */
                block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
                ASSERT(block_bitmap_idx != -1);
                bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);

                all_blocks[12] = block_lba;
                /* 把新分配的第0个间接块地址写入一级间接块表 */
                ide_write(cur_part->my_disk,  all_blocks + 12,dir_inode->i_sectors[12], 1);
            }
            else
            { // 若是间接块未分配
                all_blocks[block_idx] = block_lba;
                /* 把新分配的第(block_idx-12)个间接块地址写入一级间接块表 */
                ide_write(cur_part->my_disk,  all_blocks + 12,dir_inode->i_sectors[12], 1);
            }

            /* 再将新目录项p_de写入新分配的间接块 */
            memset(io_buf, 0, 512);
            memcpy(io_buf, p_de, dir_entry_size);
            ide_write(cur_part->my_disk, io_buf,all_blocks[block_idx],  1);
            dir_inode->i_size += dir_entry_size;
            return true;
        }

        /* 若第block_idx块已存在,将其读进内存,然后在该块中查找空目录项 */
        ide_read(cur_part->my_disk,  io_buf, all_blocks[block_idx],1);
        /* 在扇区内查找空目录项 */
        uint8_t dir_entry_idx = 0;
        while (dir_entry_idx < dir_entrys_per_sec)
        {
            if ((dir_e + dir_entry_idx)->f_type == FT_UNKNOWN)
            { // FT_UNKNOWN为0,无论是初始化或是删除文件后,都会将f_type置为FT_UNKNOWN.
                memcpy(dir_e + dir_entry_idx, p_de, dir_entry_size);
                ide_write(cur_part->my_disk, io_buf,all_blocks[block_idx],  1);

                dir_inode->i_size += dir_entry_size;
                return true;
            }
            dir_entry_idx++;
        }
        block_idx++;
    }
    printk("directory is full!\n");
    return false;
}
bool delete_dir_entry(struct partition* part, struct dir* pdir, uint32_t inode_no, void* io_buf)
{
    struct inode* dir_inode = pdir->inode;
    uint32_t block_idx = 0, all_blocks[140] = {0};
	while (block_idx<12)
	{
        all_blocks[block_idx] = pdir->inode->i_sectors[block_idx];
    }
	if (dir_inode->i_sectors[12]!=0)
	{
        ide_read(part->my_disk, all_blocks + 12, dir_inode->i_sectors[12],1);
    }
	uint32_t dir_entry_size = part->sb->dir_entry_size;
   uint32_t dir_entrys_per_sec = (SECTOR_SIZE / dir_entry_size);       // 每扇区最大的目录项数目
   struct dir_entry* dir_e = (struct dir_entry*)io_buf;   //目录项
   struct dir_entry* dir_entry_found = NULL;
   uint8_t dir_entry_idx, dir_entry_cnt;
   bool is_dir_first_block = false;     // 目录的第1个块
   block_idx = 0;
   while (block_idx<140)
   {
		is_dir_first_block = false; 
		if (all_blocks[block_idx]==0)
		{
            block_idx++;
			continue;
        }
		dir_entry_idx = dir_entry_cnt = 0;
        memset(io_buf, 0, SECTOR_SIZE);
        ide_read(part->my_disk, io_buf, all_blocks[block_idx], 1);
		while (dir_entry_idx<dir_entrys_per_sec)
		{
			if ((dir_e+dir_entry_idx)->f_type!=FT_UNKNOWN)
			{
				if (!strcmp((dir_e + dir_entry_idx)->filename, "."))//访问第一个扇区就找到待删除的项时is_dir_first_block为true,后面的扇区会被改为false且不会再进入该循环
				{
                    is_dir_first_block = true;        
                } else if (strcmp((dir_e + dir_entry_idx) ->filename,".") &&strcmp((dir_e + dir_entry_idx) ->filename,".."))//排除..和.这两个目录项不能被删除 
                {
                    dir_entry_cnt++;                 // 统计此扇区内的目录项个数,用来判断删除目录项后是否回收该扇区
					if ((dir_e + dir_entry_idx)->i_no ==inode_no)//如果是需要删除的i节点
                    {            
                        ASSERT(dir_entry_found == NULL);           
                        dir_entry_found = dir_e + dir_entry_idx;            
                    }
                }
            	dir_entry_idx++;
        	}	
   		}
		/*若次扇区未找到该目录项，继续在下一个扇区找*/
    	if (dir_entry_found == NULL) {
    		block_idx++;
 	 		continue;
		}
		ASSERT(dir_entry_cnt >= 1);
		if (!is_dir_first_block&&dir_entry_cnt==1)//如果不是目录的第一个扇区，且只有这一个目录项，将目录所占的磁盘块回收
		{
			//先将当前目录项所在的磁盘块回收
			uint32_t block_bitmap_idx = all_blocks[block_idx] - part->sb->data_start_lba;
      		bitmap_set(&part->block_bitmap, block_bitmap_idx, 0);
      		bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
			//修改inode中块索引内容
            if (block_idx<12)//直接块
			{
                dir_inode->i_sectors[block_idx] = 0;
            }else
			{
				//如果是间接块，先统计当前间接块的数量
				uint32_t indirect_blocks = 0;
	    		uint32_t indirect_block_idx = 12;
				while (indirect_block_idx<140){
					if (all_blocks[indirect_block_idx]!=0){
                        indirect_blocks++;                    
					}
                    indirect_block_idx++;
                }
				ASSERT(indirect_blocks >= 1);  // 包括当前间接块
				if (indirect_blocks>1)//如果删除后仍需使用间接块，则只修改间接索引内容
				{
                    all_blocks[block_idx] = 0;
                    ide_write(part->my_disk, all_blocks + 12,dir_inode->i_sectors[12], 1);
                } else // 否则将第12块间接块释放
                {    block_bitmap_idx = dir_inode->i_sectors[12] - part->sb->data_start_lba;
          			bitmap_set(&part->block_bitmap, block_bitmap_idx, 0);
          			bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
          			/* 将间接索引表地址清 0 */
          			dir_inode->i_sectors[12] = 0;                             
                }           
            }
        }else//如果目录中还有其他目录项，之清空这一项的内容
		{
            memset(dir_entry_found, 0, dir_entry_size);
            ide_write(part->my_disk, io_buf, all_blocks[block_idx], 1);
        }
        //更新目录对应的信息inode信息
		ASSERT(dir_inode->i_size >= dir_entry_size);
        dir_inode->i_size -= dir_entry_size;
        memset(io_buf, 0, SECTOR_SIZE * 2);
      	inode_sync(part, dir_inode, io_buf);
        return false;
   }
        /* 所有块中未找到则返回false,若出现这种情况应该是serarch_file出错了 */
   return false;
}
struct dir_entry* dir_read(struct dir*  dir)
{
    struct dir_entry* dir_e = (struct dir_entry*)dir->dir_buf;
    uint32_t all_blocks[140] = {0};
    uint32_t block_idx = 0,block_cnt=12,cur_dir_entry_pos = 0,dir_entry_idx;
    while (block_idx<12)
    {
        all_blocks[block_idx] = dir->inode->i_sectors[block_idx];
        block_idx++;
    }
    if (dir->inode->i_sectors[12]!=0)
    {
        ide_read(cur_part->my_disk, all_blocks + 12, dir->inode->i_sectors[12],1);
        block_cnt = 140;
    }
    block_idx = 0;
    uint32_t dir_entry_size = cur_part->sb->dir_entry_size;
    uint32_t dir_entrys_per_sec = SECTOR_SIZE / dir_entry_size;
    while (block_idx<block_cnt)
    {
        if (dir->dir_pos>dir->inode->i_size)//如果已经超出目录范围
        {
            
        }
        if (all_blocks[block_idx]==0)//如果索引尚未分配内存块则直接跳过
        {
            block_idx++;
            continue;
        }
        ide_read(cur_part, dir_e, all_blocks[block_idx], 1);//读取该扇区中的目录项到缓冲区中
        while (dir_entry_idx<dir_entrys_per_sec) {
            if ((dir_e + dir_entry_idx)->f_type != FT_UNKNOWN)
            {
                if (cur_dir_entry_pos<=dir->dir_pos) //如果小于上次的偏移
                {
                    cur_dir_entry_pos+=dir_entry_size;
                    dir_entry_idx++;
                    continue;
                }
                ASSERT(cur_dir_entry_pos == dir->dir_pos);//更新上次的偏移量
                dir->dir_pos += dir_entry_size;
                return dir_e + dir_entry_idx;
            }
            dir_entry_idx++;
        }
        block_idx++;
    }
    return NULL;
}
bool dir_is_empty(struct dir* dir)
{
    //如果只有.和..说明目录已经空了
    struct inode* dir_inode = dir->inode;
    return (dir->inode->i_size == cur_part->sb->dir_entry_size * 2);
}
int32_t dir_remove(struct dir* parent_dir, struct dir* chile_dir)
{
    struct inode* chile_dir_inode = chile_dir->inode;
    int32_t block_idx = 1;
    while (block_idx<13)
    {
        ASSERT(chile_dir_inode->i_sectors[block_idx] == 0);
        block_idx++;
    }
    void* io_buf = sys_malloc(SECTOR_SIZE * 2);
    if (io_buf==NULL)
    {
        printk("dir_remove: malloc for io_buf failed\n");
        return -1;
    }
    delete_dir_entry(cur_part, parent_dir, chile_dir_inode->i_no, io_buf);//删除掉在父目录里的目录项
    inode_release(cur_part, chile_dir_inode->i_no);//关闭inode结构
    sys_free(io_buf);
    return 0;
}