#include "fs.h"
#include "console.h"
#include "debug.h"
#include "dir.h"
#include "file.h"
#include "global.h"
#include "ide.h"
#include "inode.h"
#include "list.h"
#include "memory.h"
#include "printk.h"
#include "stdint.h"
#include "string.h"
#include "super_block.h"
struct partition* cur_part;  // 默认情况下操作的是哪个分区

static void partition_format(struct partition* part) {
    /* blocks_bitmap_init(为方便实现,一个块大小是一扇区)*/
    // 计算每一个模块所要占用的扇区数
    uint32_t boot_sector_sects = 1;
    uint32_t super_block_sects = 1;
    uint32_t inode_bitmap_sects =
        DIV_ROUND_UP(MAX_FILES_PER_PART, BITS_PER_SECTOR);

    uint32_t inode_table_sects =
        DIV_ROUND_UP((sizeof(struct inode) * MAX_FILES_PER_PART), SECTOR_SIZE);

    uint32_t used_sects = boot_sector_sects + super_block_sects +
                          inode_bitmap_sects + inode_table_sects;
    uint32_t free_sects = part->sec_cnt - used_sects;

    /************** 简单处理块位图占据的扇区数 ***************/
    /*块位图是放在可用区域内的*/
    uint32_t block_bitmap_sects;
    block_bitmap_sects = DIV_ROUND_UP(free_sects, BITS_PER_SECTOR);
    uint32_t block_bitmap_bit_len =
        free_sects - block_bitmap_sects;  // 除去块位图占用的扇区
    // 二次计算
    block_bitmap_sects = DIV_ROUND_UP(block_bitmap_bit_len, BITS_PER_SECTOR);

    /*超级块初始化*/
    struct super_block sb;
    sb.magic = 0x19590318;
    sb.sec_cnt = part->sec_cnt;
    sb.inode_cnt = MAX_FILES_PER_PART;
    sb.part_lba_base = part->start_lba;

    sb.block_bitmap_lba = sb.part_lba_base + 2;  // 第一个引导块，第二个为超级快
    sb.block_bitmap_sects = block_bitmap_sects;

    sb.inode_bitmap_lba = sb.block_bitmap_lba + sb.block_bitmap_sects;
    sb.inode_bitmap_sects = inode_bitmap_sects;

    sb.inode_table_lba = sb.inode_bitmap_lba + sb.inode_bitmap_sects;
    sb.inode_table_sects = inode_table_sects;

    sb.data_start_lba = sb.inode_table_lba + sb.inode_table_sects;
    sb.root_inode_no = 0;
    sb.dir_entry_size = sizeof(struct dir_entry);

    printk("%s info:\n", part->name);

    printk("magic : 0x%x\n", sb.magic);
    printk("part_lba_base : 0x%x\n", sb.part_lba_base);
    printk("all_sectors : 0x%x\n", sb.sec_cnt);
    printk("inode_cnt : 0x%x\n", sb.inode_cnt);
    printk("block_bitmap_lba : 0x%x\n", sb.block_bitmap_lba);
    printk("block_bitmap_sectors : 0x%x\n ", sb.block_bitmap_sects);
    printk("inode_bitmap_lba : 0x%x\n", sb.inode_bitmap_lba);
    printk("inode_bitmap_sectors : 0x%x\n", sb.inode_bitmap_sects);
    printk("inode_table_lba : 0x%x\n", sb.inode_table_lba);
    printk("inode_table_sectors : 0x%x\n", sb.inode_table_sects);
    printk("data_start_lba : 0x%x\n", sb.data_start_lba);

    struct disk* hd = part->my_disk;

    /*1. 将超级块写入本分区的1扇区*/
    ide_write(hd, &sb, part->start_lba + 1, 1);
    printk("super_block_lba:0x%x\n", part->start_lba + 1);

    /*找数据量最大的元信息，用其尺寸做存储缓冲区(存放位图)*/
    uint32_t buf_size = (sb.block_bitmap_sects >= sb.inode_bitmap_sects
                             ? sb.block_bitmap_sects
                             : sb.inode_bitmap_sects);

    buf_size =
        (buf_size >= sb.inode_table_sects ? buf_size : inode_table_sects) *
        SECTOR_SIZE;

    uint8_t* buf = (uint8_t*)sys_malloc(buf_size);

    /*2. 将块位图初始化并写入sb.block_bitmap_lab*/
    buf[0] |= 0x01;  // 将第0个块预留给根目录，位图中先占位置
    uint32_t block_bitmap_last_byte = block_bitmap_bit_len / 8;
    uint8_t block_bitmap_last_bit = block_bitmap_bit_len % 8;
    uint32_t last_size = SECTOR_SIZE - (block_bitmap_last_byte % SECTOR_SIZE);

    // 最后一个位图多于空间全置1(先忽略bit)
    memset(&buf[block_bitmap_last_byte], 0xff, last_size);
    /*在补上bit*/
    uint8_t bit_idx = 0;
    while (bit_idx <= block_bitmap_last_bit) {
        buf[block_bitmap_last_byte] &= ~(1 << bit_idx++);
    }

    ide_write(hd, buf, sb.block_bitmap_lba, sb.block_bitmap_sects);

    // 3 将 inode 位图初始化并写入 sb.inode_bitmap_lba
    // 先清空缓冲区
    memset(buf, 0, buf_size);
    buf[0] |= 0x1;
    // inode一个4096个刚好占用一个扇区
    ide_write(hd, buf, sb.inode_bitmap_lba, sb.inode_bitmap_sects);

    // 4 将 inode 数组初始化并写入 sb.inode_table_lba
    memset(buf, 0, buf_size);
    // 写入第0个inode(第0个指向根目录)
    struct inode* i = (struct inode*)buf;
    i->i_size = sb.dir_entry_size * 2;  // .和..
    i->i_no = 0;  // 根目录占inode数组中的第0个inode
    i->i_sectors[0] = sb.data_start_lba;

    ide_write(hd, buf, sb.inode_table_lba, sb.inode_table_sects);

    // 5 将根目录写入 sb.data_start_lba
    // 初始化根目录的两个目录项 . 和 ..
    memset(buf, 0, buf_size);
    struct dir_entry* p_de = (struct dir_entry*)buf;
    // 初始化.
    memcpy(p_de->filename, ".", 1);
    p_de->i_no = 0;
    p_de->f_type = FT_DIRECTORY;
    p_de++;
    // 初始化 ..
    memcpy(p_de->filename, "..", 2);
    p_de->i_no = 0;  // 根目录的父目录依然是根目录自己
    p_de->f_type = FT_DIRECTORY;
    // 第一个块已经预留给根目录
    ide_write(hd, buf, sb.data_start_lba, 1);
    printk("root_dir_lba:0x%x\n", sb.data_start_lba);
    printk("%s format done\n", part->name);
    sys_free(buf);
}

static bool mount_partition(struct list_node* pelem, int arg) {
    char* part_name = (char*)arg;
    struct partition* part = elem2entry(struct partition, part_tag, pelem);
    if (!strcmp(part->name, part_name)) {
        cur_part = part;
        struct disk* hd = cur_part->my_disk;

        /*sd_buf用来存储从硬盘上读入的超级块*/
        struct super_block* sb_buf =
            (struct super_block*)sys_malloc(SECTOR_SIZE);
        /* 在内存中创建分区 cur_part 的超级块 */
        cur_part->sb =
            (struct super_block*)sys_malloc(sizeof(struct super_block));
        if (cur_part->sb == NULL) {
            sys_free(sb_buf);
            PANIC("alloc memory failed!");
        }
        /*读入超级块*/
        memset(sb_buf, 0, SECTOR_SIZE);
        ide_read(hd, sb_buf, cur_part->start_lba + 1, 1);
        /* 把 sb_buf 中超级块的信息复制到分区的超级块 sb 中 */
        memcpy(cur_part->sb, sb_buf, sizeof(struct super_block));

        /*读取块位图*/
        cur_part->block_bitmap.bits =
            (uint8_t*)sys_malloc(sb_buf->block_bitmap_sects * SECTOR_SIZE);
        if (cur_part->block_bitmap.bits == NULL) {
            PANIC("alloc memory failed!");
        }
        cur_part->block_bitmap.bitmap_len =
            sb_buf->block_bitmap_sects * SECTOR_SIZE;
        // 从磁盘中读取块位图
        ide_read(hd, cur_part->block_bitmap.bits, sb_buf->block_bitmap_lba,
                 sb_buf->block_bitmap_sects);

        /**********将硬盘上的 inode 位图读入到内存************/
        cur_part->inode_bitmap.bits =
            (uint8_t*)sys_malloc(sb_buf->inode_bitmap_sects * SECTOR_SIZE);
        if (cur_part->inode_bitmap.bits == NULL) {
            PANIC("alloc memory failed!");
        }
        cur_part->inode_bitmap.bitmap_len =
            sb_buf->inode_bitmap_sects * SECTOR_SIZE;
        ide_read(hd, cur_part->inode_bitmap.bits, sb_buf->inode_bitmap_lba,
                 sb_buf->inode_bitmap_sects);

        list_init(&cur_part->open_inodes);
        printk("mount %s done!\n", part->name);

        sys_free(sb_buf);
        // 此处是为了配合 list_traversal
        return true;
    }
    return false;
}
char* path_parse(char* pathname, char* name_store) {
    if (pathname[0] == '/') {  // 根目录不需要单独解析
        while (*(++pathname) == '/')
            ;
    }

    /*开始一般的路径解析*/
    while (*pathname != '/' && *pathname != 0) {
        *name_store++ = *pathname++;
    }

    if (pathname[0] == 0) {  // 若路径字符串为空,则返回 NULL
        return NULL;
    }

    return pathname;
}

/*返回路径深度，比如/a/b/c，深度为3*/
int32_t path_depth_cnt(char* pathname) {
    ASSERT(pathname != NULL);
    char* p = pathname;
    char name[MAX_FILE_NAME_LEN];
    uint32_t depth = 0;

    /*解析路径，从中拆分出各级名称*/
    p = path_parse(p, name);
    while (name[0] != 0) {
        depth++;
        // printk("%s\n",name);
        memset(name, 0, MAX_FILE_NAME_LEN);
        printk("%s\n", name);
        if (p) {
            p = path_parse(p, name);
        }
    }

    return depth;
}

// 初始化文件系统
void filesys_init() {
    uint8_t channel_no = 0, dev_no = 0, part_idx = 0;
    /*sb_buf用来存储从硬盘上读入的超级块*/
    struct super_block* sb_buf =
        (struct super_block*)sys_malloc(sizeof(struct super_block));

    if (sb_buf == NULL) {
        PANIC("filesys_init alloc memory failed!...\n");
    }

    while (channel_no < channel_cnt) {
        dev_no = 0;
        while (dev_no < 2) {
            if (dev_no == 0) {  // 跨过主盘(裸盘)
                dev_no++;
                continue;
            }
            struct disk* hd = &channels[channel_no].devices[dev_no];
            struct partition* part = hd->mian_parts;
            while (part_idx < 12) {  // 4(主)+8(逻辑)
                if (part_idx == 4) {
                    part = hd->logic_parts;
                }

                if (part->sec_cnt != 0) {
                    memset(sb_buf, 0, SECTOR_SIZE);
                    // 读取超级块，根据魔术来判断是否存在文件系统
                    ide_read(hd, sb_buf, part->start_lba + 1, 1);
                    if (sb_buf->magic == 0x19590318) {
                        printk("%s has filesystem\n", part->name);
                    } else {
                        printk("formatting %s`s partition %s......\n", hd->name,
                               part->name);
                        partition_format(part);
                    }
                }
                part_idx++;
                part++;
            }
            dev_no++;  // 下一个磁盘
        }
        channel_no++;  // 下一个通道
    }
    sys_free(sb_buf);

    /*确认默认操作的分区*/
    char default_part[8] = "sdb1";

    /*挂载分区*/
    list_traversal(&partition_list, mount_partition, (int)default_part);

    // 将当前分区跟目录打开
    open_root_dir(cur_part);

    /* 初始化文件表 */
    uint32_t fd_idx = 0;
    while (fd_idx < MAX_FILE_OPEN) {
        file_table[fd_idx++].fd_inode = NULL;
    }
    printk("searching filesystem......\n");
}
static int search_file(const char* pathname,
                       struct path_search_record* searched_record) {
    if (!strcmp(pathname, "/") || !strcmp(pathname, "/.") ||
        !strcmp(pathname, "/..")) {
        searched_record->parent_dir = &root_dir;  //
        searched_record->file_type = FT_DIRECTORY;
        return 0;
    }
    uint32_t path_len = strlen(pathname);  // 最大查找长度

    ASSERT(pathname[0] == '/' && path_len > 1 && path_len < MAX_FILE_NAME_LEN);
    char* sub_path = (char*)pathname;
    struct dir* parent_dir = &root_dir;
    struct dir_entry dir_e;
    // 用来记录解析出的路径名
    char name[MAX_FILE_NAME_LEN];
    searched_record->parent_dir = parent_dir;
    searched_record->file_type = FT_UNKNOWN;
    uint32_t parent_inode_no = 0;
    sub_path = path_parse(sub_path, name);
    while (name[0]) {
        ASSERT(strlen(searched_record->searched_path) <
               512);  // 判断路径名是不是小于512
        strcat(searched_record->searched_path,
               "/");  // 如果是目录将解析出的内容追加到路径名之中
        strcat(searched_record->searched_path, name);
        if (search_dir_entry(cur_part, parent_dir, name,
                             &dir_e))  // 如果目录项存在
        {
            memset(name, 0, MAX_FILE_NAME_LEN);
            if (sub_path) {
                sub_path = path_parse(sub_path, name);  // 如果没到结尾接着解析
            }
            if (dir_e.f_type == FT_DIRECTORY) {
                parent_inode_no = parent_dir->inode->i_no;
                dir_close(parent_dir);
                parent_dir = dir_open(cur_part, dir_e.i_no);  // 更新父目录
                searched_record->parent_dir = parent_dir;
                continue;
            } else if (dir_e.f_type = FT_REGULAR)  // 如果这项是文件
            {
                searched_record->file_type =
                    FT_REGULAR;  // 如果是普通文件就直接返回文件的inode号
                return dir_e.i_no;
            }
        } else {
            return -1;  // 如果目录项不存在
        }
    }
    // 如果路径最后一项依然是目录
    dir_close(searched_record->parent_dir);
    searched_record->parent_dir = dir_open(cur_part, parent_inode_no);
    searched_record->file_type = FT_DIRECTORY;
    return dir_e.i_no;  // 如果最后还是目录，返回目录的inode号
}
int32_t sys_open(const char* pathname, uint8_t flags) {
    /* 对目录要用 dir_open,这里只有 open 文件 */
    if (pathname[strlen(pathname) - 1] == '/') {
        printk("can't open a directory %s \n", pathname);
        return -1;
    }
    ASSERT(flags <= 7);
    int32_t fd = -1;  // 默认找不见
    struct path_search_record searched_record;
    memset(&searched_record, 0, sizeof(struct path_search_record));
    /*记录目录深度，帮助判断中间某个目录不存在的情况*/
    uint32_t pathname_depth = path_depth_cnt((char*)pathname);
    /*先检查文件是否存在*/

    int inode_no = search_file(pathname, &searched_record);
    printk("sys_open\n");
    bool found = inode_no != -1 ? true : false;

    if (searched_record.file_type == FT_DIRECTORY) {
        printk(
            "can`t open a direcotry with open(), use opendir() to "
            "instead\n");
        dir_close(searched_record.parent_dir);
        return -1;
    }

    uint32_t path_search_depth = path_depth_cnt(searched_record.searched_path);

    /* 先判断是否把 pathname 的各层目录都访问到了,即是否在某个中间目录就失败了
     */
    if (pathname_depth != path_search_depth) {  // 中间失败
        printk("cannot access %s: Not a directory, subpath %s is't exist\n",
               pathname, searched_record.searched_path);
        dir_close(searched_record.parent_dir);
        return -1;
    }

    /* 若是在最后一个路径上没找到,并且并不是要创建文件,直接返回-1 */
    if (!found && !(flags & O_CREAT)) {
        printk("in path %s, file %s is`t exist\n",
               searched_record.searched_path,
               (strrchr(searched_record.searched_path, '/') + 1));
        dir_close(searched_record.parent_dir);
        return -1;
    } else if (found && flags & O_CREAT) {
        printk("%s has already exist!\n", pathname);
        dir_close(searched_record.parent_dir);
        return -1;
    }

    switch (flags & O_CREAT) {
        case O_CREAT:
            printk("creating file\n");
            fd = file_create(searched_record.parent_dir,
                             (strrchr(pathname, '/') + 1), flags);
            dir_close(searched_record.parent_dir);
            break;
        default:
            /* 其余情况均为打开已存在文件:
             * O_RDONLY,O_WRONLY,O_RDWR */
            fd = file_open(inode_no, flags);
            break;
    }
    return fd;
}
static uint32_t fd_local2global(uint32_t local_fd) {
    struct task_pcb* cur = running_thread();
    int32_t global_fd = cur->fd_table[local_fd];
    ASSERT(global_fd >= 0 && global_fd < MAX_FILE_OPEN);
    return (uint32_t)global_fd;
}
int32_t sys_close(int32_t fd) {
    int32_t ret = -1;
    if (fd > 2) {
        uint32_t _fd = fd_local2global(fd);
        ret = file_close(&file_table[_fd]);
        running_thread()->fd_table[fd] = -1;
    }
    return ret;
}
int32_t sys_write(int32_t fd, const void* buf, uint32_t count) {
    if (fd < 0)  // 如果是非法文件描述符返回失败
    {
        printk("sys_write: fd error\n");
        return -1;
    }
    if (fd == stdout_no) {
        char tmp[1024] = {0};
        memcpy(tmp, buf, count);
        console_put_str(tmp);
        return count;
    }
    uint32_t _fd =
        fd_local2global(fd);  // 将当前线程的私有文件描述符转化成全局描述符
    struct file* wr_file = &file_table[_fd];  // 通过全局文件打开表获取文件结构体
    if (wr_file->fd_flag & O_WRONLY || wr_file->fd_flag & O_RDWR) {
        uint32_t bytes_written = file_write(wr_file, buf, count);
        return bytes_written;
    } else {
        console_put_str(
            "sys_write: not allowed to write file without flag O_RDWR or "
            "O_WRONLY\n");
        return -1;
    }
}
int32_t sys_read(int32_t fd, void* buf, uint32_t count) {
    ASSERT(buf == NULL);
    if (fd < 0) {
        printk("sys_read: fd error\n");
        return -1;
    }
    uint32_t _fd = fd_local2global(fd);
    return file_read(&file_table[_fd], buf, count);
}
int32_t sys_lseek(int32_t fd, int32_t offset, uint8_t whence) {
    if (fd < 0) {
        printk("sys_lseek: fd error\n");
        return -1;
    }
    ASSERT(whence > 0 && whence < 4);
    uint32_t _fd = fd_local2global(fd);
    struct file* pf = &file_table[_fd];
    int32_t new_pos = 0;  // 新的偏移量必须位于文件大小之内
    int32_t file_size = (int32_t)pf->fd_inode->i_size;
    switch (whence) {
        /* SEEK_SET 新的读写位置是相对于文件开头再增加offset个位移量 */
        case SEEK_SET:
            new_pos = offset;
            break;

        /* SEEK_CUR 新的读写位置是相对于当前的位置增加offset个位移量 */
        case SEEK_CUR:  // offse可正可负
            new_pos = (int32_t)pf->fd_pos + offset;
            break;

        /* SEEK_END 新的读写位置是相对于文件尺寸再增加offset个位移量 */
        case SEEK_END:  // 此情况下,offset应该为负值
            new_pos = file_size + offset;
    }
    if (new_pos < 0 || new_pos > (file_size - 1)) {
        return -1;
    }
    pf->fd_pos = new_pos;
    return pf->fd_pos;
}