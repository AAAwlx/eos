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
static void partition_format(struct partition *part)
{
    uint32_t boot_sector_sects = 1;
    uint32_t super_block_sects = 1;//超级块占用的大小
    uint32_t inode_bitmap_sects = DIV_ROUND_UP(MAX_FILE_NAME_LEN,BITS_PER_SECTOR);//inode位图的大小
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
    sb.block_bitmap_lba = sb.part_lba_base + 2;//跳过第0mbr和第1块超级块
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
    struct disk* hd;
}