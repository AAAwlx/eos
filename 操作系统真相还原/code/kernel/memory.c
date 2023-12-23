#include "memory.h"
#include "../lib/stdint.h"
#include "../lib/string.h"
#include "debug.h"
#include "global.h"
#include "print.h"
#define PG_SIZE 4096
#define MEM_BITMAP_BASE 0xc009a000
#define PDE_IDX(addr) ((addr & 0xffc00000) >> 22)
#define PTE_IDX(addr) ((addr & 0x003ff000) >> 12)
#define KERNEL_V_START 0Xc0100000        // 将前1MB的内存绕过
struct mem_pool kernel_pool, user_pool;  // 生成内核内存池和用户内存池
struct virtual_addr kernel_vaddr;  // 此结构是用来给内核分配虚拟地址
struct mem_pool {
    struct bitmap pool_bitmap;
    uint32_t phy_addr_start;
    uint32_t pool_size;
};
uint32_t* pte_ptr(uint32_t vaddr) {
    uint32_t* pte = (uint32_t*)(0xffc00000 + ((vaddr & 0xffc00000) >> 10) +
                                PTE_IDX(vaddr) * 4);
    return pte;
}
uint32_t* pde_ptr(uint32_t vaddr) {
    uint32_t* pde = (uint32_t*)(0xffffff000 + PDE_IDX(vaddr) * 4);
    return pde;
}
void* vaddr_get(enum pool_flags pf, uint32_t pg_cnt) {
    int vaddr_start = 0, bit_idx_start = -1;
    uint32_t cnt = 0;
    if (pf == PF_KERNEL) {  // 如果是为内核分配内存
        bit_idx_start =
            bitmap_scan(&kernel_vaddr.vaddr_bitmap, pg_cnt);  // 寻找连续的位图
        while (cnt < pg_cnt) {
            cnt++;
            bitmap_set(&kernel_vaddr.vaddr_bitmap, bit_idx_start + cnt,
                       1);  // 找到后将已分配的位图位置都标记为1
        }
        if (bit_idx_start == -1) {
            return NULL;
        }
        vaddr_start = kernel_vaddr.vaddr_start + bit_idx_start * PG_SIZE;
    }

    return (void*)vaddr_start;
}
void* palloc(struct mem_pool* pool)  // 分配物理页，每次只分配一页
{
    uint32_t phy_start = 0;
    int bit_idx_start = (&kernel_vaddr.vaddr_bitmap, 1);
    if (bit_idx_start == -1) {
        return NULL;
    }
    bitmap_set(&pool->pool_bitmap, bit_idx_start, 1);
    phy_start = pool->phy_addr_start + bit_idx_start * PG_SIZE;
    put_str("palloc ");
    put_int(phy_start);
    return (void*)phy_start;
}
static void page_table_add(
    void* _vaddr,
    void* _page_phyaddr)  // 将物理地址写入虚拟地址对应的页表内
{
    uint32_t vaddr = (uint32_t)_vaddr, page_phyaddr = (uint32_t)_page_phyaddr;
    uint32_t* pte = pte_ptr(_vaddr);
    uint32_t* pde = pde_ptr(_vaddr);
    if (*pde & 0x00000001)  // 比较看当前页目录项（该页表）是否存在
    {
        ASSERT(!(*pte & 0x00000001));
        if (!(*pte & 0x00000001)) {
            // 只要是创建页表,pte 就应该不存在,多判断一下放心
            *pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
            put_int(*pte);
        } else {  // 目前应该不会执行到这,因为上面的 ASSERT 会先执行
            PANIC("pte repeat");
            // *pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);  //
        }
    } else {  // 若页表项不存在
        uint32_t pde_phyaddr = (uint32_t)palloc(&kernel_pool);
        put_str("pde_phyaddr");
        put_int(pde_phyaddr);
        put_str("\n");
        *pde = (pde_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
        memset((void*)((int)pte & 0xfffff000), 0,
               PG_SIZE);  // 清空这块页表内存，防止脏页导致数据混乱
        ASSERT(!(*pte & 0x00000001));
        *pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);  // US=1,RW=1,P=1
    }
}

void* malloc_page(enum pool_flags pf, uint32_t pg_cnt) {
    ASSERT(pg_cnt > 0 &&
           pg_cnt < 3840);  // 分配的页面数不能小于0,或超过最大内存数
    void* vaddr_start = vaddr_get(pf, pg_cnt);  // 先分配虚拟地址
    void* vaddr = vaddr_start;
    if (vaddr_start == NULL) {
        return NULL;
    }
    struct mem_pool* pool = (pf & PF_KERNEL) ? &kernel_pool : &user_pool;
    while (pg_cnt-- > 0) {
        uint32_t _page_phyaddr = palloc(pool);
        if (_page_phyaddr == NULL) {
            return NULL;
        }
        page_table_add(vaddr, _page_phyaddr);
        vaddr += PG_SIZE;
    }
    return vaddr_start;
}
void* get_kernel_pages(uint32_t pg_cnt) {
    put_str("get_kernel_pages\n");
    void* vaddr = malloc_page(PF_KERNEL, pg_cnt);
    if (vaddr != NULL) {  // 若分配的地址不为空,将页框清0后返回
        memset(vaddr, 0, pg_cnt * PG_SIZE);
    }
    put_str("get_kernel_pages_down\n");
    return vaddr;
}
void mem_pool_init(uint32_t mem_bytes_total) {
    put_str(" mem_pool_init start\n");
    uint32_t page_table_size =
        256 * PG_SIZE;  // 之前被使用过的第769～1023页目录项分配的页表占用的内存
    uint32_t used_mem = page_table_size + 0x100000;  // 之前总共使用掉的所有内存
    uint32_t all_mem = mem_bytes_total - used_mem;  //
    uint32_t use_phy_mem = all_mem / 2;
    uint32_t ken_phy_mem = all_mem - use_phy_mem;
    uint32_t ken_phy_start = used_mem;
    uint32_t use_phy_start = used_mem + ken_phy_mem;
    uint32_t ken_phy_bitmap_length = ken_phy_mem / (8 * PG_SIZE);
    uint32_t use_phy_bitmap_length = use_phy_mem / (8 * PG_SIZE);
    kernel_pool.phy_addr_start = ken_phy_start;
    kernel_pool.pool_size = ken_phy_mem;
    kernel_pool.pool_bitmap.bitmap_len = ken_phy_bitmap_length;
    kernel_pool.pool_bitmap.bits = (void*)MEM_BITMAP_BASE;
    user_pool.phy_addr_start = use_phy_start;
    user_pool.pool_bitmap.bitmap_len = use_phy_bitmap_length;
    user_pool.pool_size = use_phy_mem;
    user_pool.pool_bitmap.bits = (void*)MEM_BITMAP_BASE + ken_phy_bitmap_length;
    put_str("kernel_pool_bitmap_start:");
    put_int((int)kernel_pool.pool_bitmap.bits);
    put_str(" kernel_pool_phy_addr_start:");
    put_int(kernel_pool.phy_addr_start);
    put_str("\n");
    put_str("user_pool_bitmap_start:");
    put_int((int)user_pool.pool_bitmap.bits);
    put_str(" user_pool_phy_addr_start:");
    put_int(user_pool.phy_addr_start);
    put_str("\n");
    bitmap_init(&kernel_pool.pool_bitmap);
    bitmap_init(&user_pool.pool_bitmap);
    kernel_vaddr.vaddr_start = KERNEL_V_START;
    kernel_vaddr.vaddr_bitmap.bitmap_len = ken_phy_bitmap_length;
    kernel_vaddr.vaddr_bitmap.bits =
        (void*)(ken_phy_bitmap_length + use_phy_bitmap_length +
                MEM_BITMAP_BASE);
    bitmap_init(&kernel_vaddr.vaddr_bitmap);
    put_str("mem_pool_init done\n");
}
void mem_init() {
    put_str("mem_init start\n");
    uint32_t mem_bytes_total = (*(
        uint32_t*)(0xb00));  // 将先前loade.s中统计好的内存总量读出并交给变量mem_bytes_total
    mem_pool_init(mem_bytes_total);  // 初始化内存池
    put_str("mem_init done\n");
}
