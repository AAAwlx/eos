#include "ide.h"
#include "../lib/stdio.h"
#include "debug.h"
#include "interrupt.h"
#include "io.h"
#include "timer.h"
// 定义各个端口
#define reg_data(channel) (channel->port_base + 0)
#define reg_error(channel) (channel->port_base + 1)
#define reg_sect_cnt(channel) (channel->port_base + 2)
#define reg_lba_l(channel) (channel->port_base + 3)
#define reg_lba_m(channel) (channel->port_base + 4)
#define reg_lba_h(channel) (channel->port_base + 5)
#define reg_dev(channel) (channel->port_base + 6)
#define reg_status(channel) (channel->port_base + 7)
#define reg_cmd(channel) (reg_status(channel))
#define reg_alt_status(channel) (channel->port_base + 0x206)
#define reg_ctl(channel) reg_alt_status(channel)
// 硬盘状态
#define BIT_STAT_BASY 0X80   // 硬盘忙
#define BIT_STAT_REDAY 0X40  // 驱动准备好了
#define BIT_STAT_DRQ 0X8     // 数据传输准备好了
// 硬盘控制字
#define BIT_DEV_MBS 0xa0  // 第7位和第5位固定为1
#define BIT_DEV_LBA 0x40
#define BIT_DEV_DEV 0x10  // 主盘还是从盘

/* 一些硬盘操作的指令 */
#define CMD_IDENTIFY 0xec      // identify指令
#define CMD_READ_SECTOR 0x20   // 读扇区指令
#define CMD_WRITE_SECTOR 0x30  // 写扇区指令

/* 定义可读写的最大扇区数,调试用的 */
#define max_lba ((80 * 1024 * 1024 / 512) - 1)  // 只支持80MB硬盘

uint8_t channel_cnt;             // 通道数量
struct ide_channel channels[2];  // 最多支持两个通道
int32_t ext_lba_base = 0;
uint8_t p_no = 0, l_no = 0;
struct list partition_list; // 用来记录硬盘主分区和逻辑分区的下标

/* 构建一个16字节大小的结构体,用来存分区表项 */
struct partition_table_entry {
    uint8_t bootable;       // 是否可引导
    uint8_t start_head;     // 起始磁头号
    uint8_t start_sec;      // 起始扇区号
    uint8_t start_chs;      // 起始柱面号
    uint8_t fs_type;        // 分区类型
    uint8_t end_head;       // 结束磁头号
    uint8_t end_sec;        // 结束扇区号
    uint8_t end_chs;        // 结束柱面号
                            /* 更需要关注的是下面这两项 */
    uint32_t start_lba;     // 本分区起始扇区的lba地址
    uint32_t sec_cnt;       // 本分区的扇区数目
} __attribute__((packed));  // 保证此结构是16字节大小

/* 引导扇区,mbr或ebr所在的扇区 */
struct boot_sector {
    uint8_t other[446];                               // 引导代码
    struct partition_table_entry partition_table[4];  // 分区表中有4项,共64字节
    uint16_t signature;  // 启动扇区的结束标志是0x55,0xaa,
} __attribute__((packed));

// 选择要读写硬盘
static void select_disk(struct disk* hd) {
    uint8_t reg_device = BIT_DEV_MBS | BIT_DEV_LBA;
    if (hd->dev_no = 1) {
        reg_device |= BIT_DEV_DEV;
    }
    outb(reg_dev(hd->mychannel), reg_device);
}
// 向硬盘输入要读写的扇区起始地址以及扇区数
static void select_sector(struct disk* hd, uint32_t lab, uint8_t cnt) {
    ASSERT(cnt <= max_lba && cnt > 0);
    // 写入读取扇区数量
    outb(reg_sect_cnt(hd->mychannel), cnt);
    // 写入28位起始地址
    outb(reg_lba_l(hd->mychannel), lab);
    outb(reg_lba_m(hd->mychannel), lab >> 8);
    outb(reg_lba_h(hd->mychannel), lab >> 16);
    outb(reg_dev(hd->mychannel), BIT_DEV_MBS |
                                     (hd->dev_no == 1 ? BIT_DEV_DEV : 0) |
                                     BIT_DEV_LBA | lab >> 24);
}
static void cmd_out(struct disk* hd, uint8_t cmd) {
    hd->mychannel->expect_intr = true;  // 为后文中断处理程序做铺垫
    outb(reg_cmd(hd->mychannel), cmd);
}
static void read_from_sector(struct disk* hd, char* buffer, uint8_t cnt) {
    uint32_t buffer_size;
    if (cnt == 0) {
        buffer_size = 256 * 512;
    } else {
        buffer_size = cnt * 512;
    }
    inw(reg_data(hd->mychannel), buffer, buffer_size/2);
}
static void write_to_sector(struct disk* hd, char* buffer, uint8_t cnt) {
    uint32_t buffer_size;
    if (cnt == 0) {
        buffer_size = 256 * 512;
    } else {
        buffer_size = cnt * 512;
    }
    outw(reg_data(hd->mychannel), buffer, buffer_size/2);
}
static bool basy_wait(struct disk* hd)
{
    uint32_t time_limit = 30 * 1000;
    while (time_limit -=10>0)
    {
        if (!((inb(reg_status(hd->mychannel)))&BIT_STAT_BASY))//如果磁盘无io
        {
            return (inb(reg_status(hd->mychannel)) & BIT_STAT_BASY);
        } else {
            mtime_sleep(10);
        }
    }
    return false;
}
void ide_read(struct disk* hd, char* buffer, uint32_t lab, uint32_t cnt)
{
    ASSERT(lab <= max_lba);
    ASSERT(cnt > 0);
    lock_acquire(&hd->mychannel->c_lock);
    select_disk(hd);
    uint32_t size_op ,size_down= 0;
    while (size_down<cnt) {
        if ((size_op+256)<=cnt)
        {
            size_op = 256;
        }else
        {
            size_op = cnt - size_down;
        }
        select_sector(hd, lab + size_down, size_op);
        cmd_out(hd, CMD_READ_SECTOR);
        //此时磁盘进入工作状态，待磁盘将数据找到并准备好后会向cpu发出中断通过中断唤醒沉睡的线程
        sema_down(&hd->mychannel->disk_down);
        if (!(basy_wait(hd)))
        {
            char error[64];
            sprintf(error, "%s read sector %d failed!!!!!!\n", hd->name, lab);
            PANIC(error);
        }
        read_from_sector(hd,(void*)((uint32_t)buffer+512*size_down), size_op);
        size_down += size_op;
    }
    lock_release(hd->mychannel->c_lock);
}
void ide_write(struct disk* hd, char* buffer, uint32_t lab, uint32_t cnt)
{
    ASSERT(lab <= max_lba);
    ASSERT(cnt > 0);
    lock_acquire(&hd->mychannel->c_lock);
    select_disk(hd);
    uint32_t size_op ,size_down= 0;
    while (size_down<cnt) {
        if ((size_op+256)<=cnt)
        {
            size_op = 256;
        }else
        {
            size_op = cnt - size_down;
        }
        select_sector(hd, lab + size_down, size_op);
        cmd_out(hd, CMD_WRITE_SECTOR);
        if (!(basy_wait(hd)))
        {
            char error[64];
            sprintf(error, "%s write sector %d failed!!!!!!\n", hd->name, lab);
            PANIC(error);
        }
        write_to_sector(hd,(void*)((uint32_t)buffer+512*size_down), size_op);
        //此时磁盘进入工作状态，待磁盘将数据找到并准备好后会向cpu发出中断通过中断唤醒沉睡的线程
        sema_down(&hd->mychannel->disk_down);
        size_down += size_op;
    }
    lock_release(hd->mychannel->c_lock);
}
void intr_hd_handler(uint8_t irq_no)
{
    ASSERT(irq_no==0x2e||irq_no==0x2f)
    uint8_t ch_no = irq_no - 0x2e;
    struct ide_channel* channel = &channels[ch_no];
    if (channel->expect_intr)
    {
        channel->expect_intr = false;
        sema_up(&channel->disk_down);
        inb(reg_status(channel));
    }
}

void ide_init() {
    printk("ide_init start\n");
    uint8_t disk_cnt = *((uint8_t*)(0x475));
    printk("   ide_init hd_cnt:%d\n", disk_cnt);
    ASSERT(disk_cnt > 0);
    channel_cnt = DIV_ROUND_UP(disk_cnt, 2);
    uint8_t channel_no = 0,dev_no = 0;
    while (channel_no < channel_cnt) {
        struct ide_channel* c = &channels[channel_no];
        sprintf(c->name, "ide%d", channel_no);
        switch (channel_no) {
            case 0:
                channels[0].port_base = 0x1f0;
                channels[0].inr_no = 0x20 + 14;
                break;
            case 1:
                channels[1].port_base = 0x170;
                channels[1].inr_no = 0x20 + 15;
                break;
        }
        c->expect_intr = false;
        lock_init(&c->c_lock);
        sema_init(&c->disk_down, 0);
        register_hsndler(c->inr_no, intr_hd_handler);
        while ()
        {
            
        }
        channel_no++;
    }
    printk("\n   all partition info\n");
    /* 打印所有分区信息 */
    list_traversal(&partition_list, partition_info, (int)NULL);
    printk("ide_init done\n");
}