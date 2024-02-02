#include "fs.h"
#include "super_block.h"
#include "inode.h"
#include "dir.h"
#include "stdint.h"
#include "printk.h"
#include "list.h"
#include "string.h"
#include "ide.h"
#include "global.h"
#include "debug.h"
#include "memory.h"

struct partition* cur_part;	 // 默认情况下操作的是哪个分区

static void partition_format(struct partition *part)
{
    uint32_t boot_sector_sects = 1;
    uint32_t super_block_sects = 1;//超级块占用的大小
    uint32_t inode_bitmap_sects = DIV_ROUND_UP(MAX_FILES_PER_PART,BITS_PER_SECTOR);//inode位图的大小
    uint32_t inode_table_sects = DIV_ROUND_UP((sizeof(struct inode)*MAX_FILES_PER_PART), SIZE_MAX);//inode块的大小
    uint32_t used_sects = boot_sector_sects + super_block_sects +
                          inode_bitmap_sects + inode_table_sects;//已经使用过的扇区数量
    uint32_t free_sects = part->sec_cnt - used_sects;//总扇区数减去被使用过的

    //块位图占据的区域
    uint32_t block_bitmap_sects;
    block_bitmap_sects = DIV_ROUND_UP(free_sects, BITS_PER_SECTOR);
    /* block_bitmap_bit_len是位图中位的长度,也是可用块的数量 */
    uint32_t block_bitmap_bit_len = free_sects - block_bitmap_sects; 
    block_bitmap_sects = DIV_ROUND_UP(block_bitmap_bit_len, BITS_PER_SECTOR);

    struct super_block sb;
    sb.magic = 0x19590318;
    sb.sec_cnt = part->sec_cnt;
    sb.inode_cnt = MAX_FILES_PER_PART;//inode的数量应该和支持的最大文件数
    sb.part_lba_base = part->start_lba;//本块起始的地址
    sb.block_bitmap_lba = sb.part_lba_base + 2;//跳过第0mbr和第1块超级块的位置
    sb.block_bitmap_sects = block_bitmap_sects;//位图块占用的扇区数量
    sb.inode_table_lba = sb.block_bitmap_lba + sb.part_lba_base;
    sb.inode_bitmap_sects = inode_bitmap_sects;
    sb.inode_table_lba = sb.inode_bitmap_lba + sb.inode_bitmap_sects;
    sb.inode_table_sects = inode_table_sects; 

    sb.data_start_lba = sb.inode_table_lba + sb.inode_table_sects;
    sb.root_inode_no = 0;
    sb.dir_entry_size = sizeof(struct dir_entry);
    printk("%s info:\n", part->name);
    printk("   magic:0x%x\n   part_lba_base:0x%x\n   all_sectors:0x%x\n   inode_cnt:0x%x\n   block_bitmap_lba:0x%x\n   block_bitmap_sectors:0x%x\n   inode_bitmap_lba:0x%x\n   inode_bitmap_sectors:0x%x\n   inode_table_lba:0x%x\n   inode_table_sectors:0x%x\n   data_start_lba:0x%x\n", sb.magic, sb.part_lba_base, sb.sec_cnt, sb.inode_cnt, sb.block_bitmap_lba, sb.block_bitmap_sects, sb.inode_bitmap_lba, sb.inode_bitmap_sects, sb.inode_table_lba, sb.inode_table_sects, sb.data_start_lba);
    struct disk* hd=part->my_disk;
    //将超级块写入到磁盘中
    ide_write(hd, part->start_lba + 1, &sb, 1);
    printk("   super_block_lba:0x%x\n", part->start_lba + 1);
    uint32_t buf_size =
        (sb.block_bitmap_sects > sb.inode_bitmap_sects ? sb.block_bitmap_sects
                                                       : sb.inode_bitmap_sects);
    //为位图分配暂存的内存缓冲区
    uint8_t* buf = (uint8_t*)sys_malloc(buf_size);

    //将块位图初始化并写入到磁盘中
    buf[0] |= 0x01;//将第0位预留出来给根目录
    uint32_t block_bitmap_last_byte = block_bitmap_bit_len / 8;//计算出块位图的字节数
    uint8_t  block_bitmap_last_bit  = block_bitmap_bit_len % 8;//没能被整除剩余的位
    uint32_t last_size=SECTOR_SIZE-(block_bitmap_last_byte % SECTOR_SIZE);//块位图结尾中最终不足一个扇区的部分
    memset(&buf[block_bitmap_last_byte], 0xff, last_size);//将扇区中空余的部分补全为1
    uint8_t bit_idx = 0;
    //将最后block_bitmap_last_bit剩余不满一字节的位数恢复为0
    while (bit_idx <= block_bitmap_last_bit) {
        buf[block_bitmap_last_byte] &= ~(1 << bit_idx++);
    }
    ide_write(hd, sb.block_bitmap_lba, buf, sb.block_bitmap_sects);

    //将inode位图写入到磁盘中
    memset(buf, 0, buf_size);
    buf[0] |= 0x1;//将首位标志位预留给根目录
    ide_write(hd, sb.inode_bitmap_lba, buf, sb.inode_table_sects);//inode号是整数倍

    //将inode号为0的文件信息写入到inode区域中,根目录u
    memset(buf, 0, buf_size);
    struct inode* i = (struct inode*)buf;
    i->i_size = sb.dir_entry_size * 2;//.目录项和 ..目录项
    i->i_no = 0;
    i->i_sectors[0] = sb.data_start_lba;//将第一个数据块指针初始化为数据区的初始处
    ide_write(hd, sb.inode_table_lba, buf, sb.inode_table_sects);

    //初始化根目录和.以及..
    memset(buf, 0, buf_size);
    struct dir_entry* p_de = (struct dir_entry*)buf;
    /* 初始化当前目录"." */
    memcpy(p_de->filename, ".", 1);
    p_de->i_no = 0;
    p_de->f_type = FT_DIRECTORY;//文件类型为目录类型
    /* 初始化当前目录".." */
    memcpy(p_de->filename, "..", 2);
    p_de->i_no = 0;
    p_de->f_type = FT_DIRECTORY;//文件类型为目录类型
    ide_write(hd, sb.data_start_lba, buf, 1);
    printk("root_dir_lba:0x%x\n", sb.data_start_lba);
    printk("%s format done\n", part->name);
    sys_free(buf);
}
static bool mount_partition(struct list_node *pelem,int arg)
{
    char* part_name = (char*)arg;//初始化分区名
    struct partition* part = elem2entry(struct partition, part_tag, pelem);
    if (!strcmp(part->name,part_name))//如果是默认的part
    {
        cur_part = part;
        struct disk* hd = part->my_disk;
        struct super_block* sb_buf =
            (struct super_block*)sys_malloc(SECTOR_SIZE);//申请缓冲区
        cur_part->sb = (struct super_block*)sys_malloc(sizeof(struct super_block));//为分区的超级块申请内存
        if (cur_part->sb==NULL)
        {
            PANIC("alloc memory failed!");
        }
        memset(sb_buf, 0, SECTOR_SIZE);
        ide_read(hd, sb_buf,cur_part->start_lba + 1,1);//将磁盘内容读入到sb_buf之中
        memcpy(cur_part->sb, sb_buf, SECTOR_SIZE);//将缓冲区中的cpy到分区超级块的结构体之中副
        //从磁盘中读出块位图
        cur_part->block_bitmap.bits =
            (uint8_t*)sys_malloc(sb_buf->block_bitmap_sects * SECTOR_SIZE);
        if (cur_part->block_bitmap.bits==NULL)
        {
            PANIC("alloc memory failed!");
        }
        cur_part->block_bitmap.bitmap_len = sb_buf->block_bitmap_sects;
        ide_read(hd,  cur_part->block_bitmap.bits,sb_buf->block_bitmap_lba,
                 sb_buf->block_bitmap_sects);
        // 从磁盘中读出inode位图
        cur_part->inode_bitmap.bits =
            (uint8_t*)sys_malloc(sb_buf->inode_bitmap_sects * SECTOR_SIZE);
        if (cur_part->inode_bitmap.bits==NULL)
        {
            PANIC("alloc memory failed!");
        }
        cur_part->block_bitmap.bitmap_len = sb_buf->block_bitmap_lba;
        ide_read(hd, cur_part->inode_bitmap.bits,sb_buf->inode_bitmap_lba, 
                 sb_buf->inode_bitmap_sects);
        list_init(&cur_part->open_inodes);
        printk("mount %s done!\n", part->name);
        return true;
    }
    return false;
}

//初始化文件系统
void filesys_init()
{
    uint8_t channel_no = 0, dev_no, part_idx = 0;
    struct super_block* sb_buf = (struct super_block*)sys_malloc(SECTOR_SIZE);
    while (channel_no<channel_cnt)
    {
        dev_no = 0;
        while (dev_no<2)
        {
            if (dev_no==0)//跳过操作系统所在的
            {
                dev_no++;
                continue;
            }
            struct disk* hd = &channels[channel_no].devices[dev_no];
            struct partition* part = hd->mian_parts;//前四个分区是
            while (part_idx<12)//遍历该磁盘上的
            {
                if (part_idx==4)
                {
                    part = hd->mian_parts;
                }
                if (part->sec_cnt!=0)
                {
                    memset(sb_buf, 0, SECTOR_SIZE);
                    ide_read(hd, part->start_lba + 1, sb_buf, 1);
                    if (sb_buf->magic==0x19590318) {
                        printk("%s has filesystem\n", part->name);
                    }else
                    {
                        printk("formatting %s`s partition %s......\n", hd->name, part->name);
                        partition_format(part);
                    }
                }
                part_idx++;
                part++;
            }
            dev_no++;
        }
        channel_no++;
    }
    sys_free(sb_buf);
    char default_part[8] = "sdb1";
    list_traversal(&partition_list, mount_partition, (int)default_part);
}