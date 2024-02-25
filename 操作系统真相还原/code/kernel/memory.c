#include "memory.h"
#include "../lib/stdint.h"
#include "../lib/string.h"
#include "debug.h"
#include "global.h"
#include "printk.h"
#include"sysnc.h"
#include"interrupt.h"
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
    struct lock lock;
};
struct arean{
    struct mem_block_desc* desc;
    uint32_t cnt;
    bool large;//若为ture则表示页框数量，若为false则表示空闲内存块的数量
};
struct mem_block_desc k_block_descs[DESC_CNT];//内核态的描述符数组
uint32_t* pte_ptr(uint32_t vaddr) {
    uint32_t* pte = (uint32_t*)(0xffc00000 + ((vaddr & 0xffc00000) >> 10) +PTE_IDX(vaddr) * 4);
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
        bit_idx_start = bitmap_scan(&kernel_vaddr.vaddr_bitmap, pg_cnt);  // 寻找连续的位图
        if (bit_idx_start == -1) {
            return NULL;
        }
        while (cnt < pg_cnt) {
            bitmap_set(&kernel_vaddr.vaddr_bitmap, bit_idx_start + cnt++,1);  // 找到后将已分配的位图位置都标记为1
        }
        vaddr_start = kernel_vaddr.vaddr_start + bit_idx_start * PG_SIZE;
    }else
    {
        struct task_pcb* cur = running_thread();
        bit_idx_start = bitmap_scan(&cur->userprog_vaddar.vaddr_bitmap, pg_cnt);  // 寻找连续的位图
        if (bit_idx_start == -1) {
            return NULL;
        }
        while (cnt < pg_cnt) {
            bitmap_set(&cur->userprog_vaddar.vaddr_bitmap, bit_idx_start + cnt++,1);  // 找到后将已分配的位图位置都标记为1
        }
        vaddr_start =cur->userprog_vaddar.vaddr_start + bit_idx_start * PG_SIZE;
        ASSERT((uint32_t)vaddr_start < (0xc0000000 - PG_SIZE));//检查虚拟地址分配是否在低3GB内，高1GB为内核代码
    }
    return (void*)vaddr_start;
}
void* palloc(struct mem_pool* pool)  // 分配物理页，每次只分配一页
{
    int bit_idx_start = bitmap_scan(&pool->pool_bitmap, 1);
    if (bit_idx_start == -1) {
        return NULL;
    }
    bitmap_set(&pool->pool_bitmap, bit_idx_start, 1);
    uint32_t phy_start = ((bit_idx_start * PG_SIZE) + pool->phy_addr_start);
    return (void*)phy_start;
}
static void page_table_add(void* _vaddr, void* _page_phyaddr) {
    uint32_t vaddr = (uint32_t)_vaddr, page_phyaddr = (uint32_t)_page_phyaddr;
    uint32_t* pde = pde_ptr(vaddr);
    uint32_t* pte = pte_ptr(vaddr);
    if (*pde &
        0x00000001) {  // 页目录项和页表项的第0位为P,此处判断目录项是否存在
        ASSERT(!(*pte & 0x00000001));

        if (!(*pte &
              0x00000001)) {  // 只要是创建页表,pte就应该不存在,多判断一下放心
            *pte =
                (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);  // US=1,RW=1,P=1
        } else {  // 应该不会执行到这，因为上面的ASSERT会先执行。
            PANIC("pte repeat");
            *pte =
                (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);  // US=1,RW=1,P=1
        }
    } else {  // 页目录项不存在,所以要先创建页目录再创建页表项.
        /* 页表中用到的页框一律从内核空间分配 */
        uint32_t pde_phyaddr = (uint32_t)palloc(&kernel_pool);

        *pde = (pde_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
        memset((void*)((int)pte & 0xfffff000), 0, PG_SIZE);

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
    //put_str("get_kernel_pages\n");
    lock_acquire(&kernel_pool.lock);
    void* vaddr = malloc_page(PF_KERNEL, pg_cnt);
    if (vaddr != NULL) {  // 若分配的地址不为空,将页框清0后返回
        memset(vaddr, 0, pg_cnt * PG_SIZE);
    }
    lock_release(&kernel_pool.lock);
    return vaddr;
}
void* get_user_pages(uint32_t pg_cnt) {
    put_str("get_user_pages\n");
    lock_acquire(&user_pool.lock);
    void* vaddr = malloc_page(PF_USER, pg_cnt);
    if (vaddr != NULL) {  // 若分配的地址不为空,将页框清0后返回
        memset(vaddr, 0, pg_cnt * PG_SIZE);
    }
    lock_release(&user_pool.lock);
    put_str("get_user_pages_down\n");
    return vaddr;
}
//一页一页的分配内存
void* get_a_page(enum pool_flags pf, uint32_t vaddr)
{
    struct mem_pool* pool = pf & PF_KERNEL ? &kernel_pool : &user_pool;
    lock_acquire(&pool->lock);
    struct task_pcb* cur = running_thread();
    int bit_idx = -1;
    if (cur->pgdir != NULL && pf == PF_USER) {
        bit_idx = (vaddr - cur->userprog_vaddar.vaddr_start) / PG_SIZE;
        ASSERT(bit_idx > 0);
        bitmap_set(&cur->userprog_vaddar.vaddr_bitmap, bit_idx, 1);
    }else if (cur->pgdir == NULL && pf == PF_KERNEL) {
        bit_idx = (vaddr - cur->userprog_vaddar.vaddr_start) / PG_SIZE;
        ASSERT(bit_idx > 0);
        bitmap_set(&kernel_vaddr.vaddr_bitmap, bit_idx, 1);
    }
    void *page_phyaddr = palloc(pool);
    if (page_phyaddr==NULL) {
        return NULL;
    }
    page_table_add((void*)vaddr, page_phyaddr);
    lock_release(&pool->lock);
    return (void*)vaddr;
}
uint32_t addr_v2p(uint32_t vaddr)
{
    uint32_t* pte = pte_ptr(vaddr);
    return ((*pte & 0xfffff000) + (vaddr & 0x00000fff));
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
void block_desc_init(struct mem_block_desc* desc_array)
{
    uint16_t desc_idx, block_size = 16;
  /*初始化每个mem_block_desc描述符*/
  for (desc_idx = 0; desc_idx < DESC_CNT; desc_idx++) {
    desc_array[desc_idx].block_size = block_size;
    desc_array[desc_idx].block_per_arena =
        (PG_SIZE - sizeof(struct arean)) / block_size;

    list_init(&desc_array[desc_idx].freelist);

    block_size *= 2;
    //printk("block_size[%d]:%d\n", desc_idx, block_size);
  }
}
//将获取内存块的地址
struct mem_block* arena2block(struct arean *a,uint32_t index)
{
    return (struct mem_block*)((uint32_t)a + sizeof(struct arean) +
                               index*a->desc->block_size);
}
struct arean* block2arena(struct mem_block *b)
{
    return (struct arean*)((uint32_t)b & 0xfffff000);
}
//申请内存块
void * sys_malloc(uint32_t size)
{
    enum pool_flags pf;//标志
    struct mem_pool* pool;//内存池大小
    struct task_pcb *pthread= running_thread();
    struct mem_block_desc* descs;//描述符数组

    uint32_t poolsize;
    if (pthread->pgdir == NULL) {
        pf = PF_KERNEL;//
        pool = &kernel_pool;
        poolsize = kernel_pool.pool_size;
        descs = k_block_descs;
    }else
    {
        pf = PF_USER;//
        pool = &user_pool;
        poolsize = user_pool.pool_size;
        descs = pthread->u_block_descs;
    }
    if (size<0||size>poolsize)//如果大于内存池或小于0
    {
        return NULL;
    }
    lock_acquire(&pool->lock);
    struct arean* a;
    struct mem_block* b;
    
    if (size > 1024) {  // 如果要分配的内存大于一个页框
        uint32_t page_cnt = DIV_ROUND_UP(size + sizeof(struct arean), PG_SIZE);      // 向上取整需要的页框数
        a = malloc_page(pf,page_cnt);
        if (a==NULL) {
            lock_release(&pool->lock);
            return NULL;
        }else
        {
            a->cnt = page_cnt;
            a->large = true;
            a->desc = NULL;
            memset(a, 0, page_cnt * PG_SIZE);
            lock_release(&pool->lock);
            return (void*)(a + 1);
        }
    } else {
        
        uint8_t i;
        for (i= 0; i < DESC_CNT; i++) {
            //printk("block_size %d:%d\n", i, descs[i].block_size);
            if (descs[i].block_size >=size)
            {
                break;
            }
        }
        if (list_empty(&descs[i].freelist)) {
            a = malloc_page(pf, 1);  // 分配1页框做为arena
            if (a == NULL) {
                lock_release(&pool->lock);
                return NULL;
            }
            memset(a, 0, PG_SIZE);

            /* 对于分配的小块内存,将desc置为相应内存块描述符,
             * cnt置为此arena可用的内存块数,large置为false */
            a->desc = &descs[i];
            a->large = false;
            a->cnt = descs[i].block_per_arena;
            uint32_t block_idx;

            enum intr_status old_status = intr_disable();

            /* 开始将arena拆分成内存块,并添加到内存块描述符的free_list中 */
            for (block_idx = 0; block_idx < descs[i].block_per_arena;
                 block_idx++) {
                b = arena2block(a, block_idx);
                ASSERT(!elem_find(&a->desc->freelist, &b->node));
                list_append(&a->desc->freelist, &b->node);
            }
            intr_set_status(old_status);
        }
        b = elem2entry(struct mem_block, node, list_pop(&descs[i].freelist));
        memset(b, 0, descs[i].block_size);
        a = block2arena(b);
        a->cnt--;
        lock_release(&pool->lock);
        return (void *)b;
    }
}
//回收物理内存页
void pfree(uint32_t phy_addr)
{
    uint32_t bit_idx;
    if (phy_addr > user_pool.phy_addr_start) {
        bit_idx = (phy_addr - user_pool.phy_addr_start) / PG_SIZE;
        bitmap_set(&user_pool.pool_bitmap, bit_idx, 0);
    } else {
        bit_idx = (phy_addr - kernel_pool.phy_addr_start) / PG_SIZE;
        bitmap_set(&kernel_pool.pool_bitmap, bit_idx, 0);
    }
}
//重置tlb表中的页表的存在位
void pagetable_remove(void *vaddr)
{
    uint32_t* p = pte_ptr(vaddr);
    *p &= ~PG_P_1;
    asm volatile ("invlpg %0" ::"m"(vaddr) : "memory");
}
//虚拟地址的回收
static void vaddr_remove(enum pool_flags pf, void* _vaddr, uint32_t pg_cnt) {
    uint32_t bit_idx_start = 0;
    if (pf == PF_KERNEL) {  // 内核虚拟内存池
        bit_idx_start = ((uint32_t)_vaddr - kernel_vaddr.vaddr_start) / PG_SIZE;
        int cnt = 0;
        while (cnt < pg_cnt) {
            bitmap_set(&kernel_vaddr.vaddr_bitmap, bit_idx_start + cnt++, 0);
        }
    } else {  // 用户虚拟内存池
       struct task_pcb* pthread = running_thread();
       int i = 0;
        bit_idx_start =
            ((uint32_t)_vaddr - pthread->userprog_vaddar.vaddr_start) / PG_SIZE;
        while (i < pg_cnt) {
            bitmap_set(&pthread->userprog_vaddar.vaddr_bitmap, bit_idx_start + i++, 0);
        }
    }
}
void mfree_page(enum pool_flags pf, void* _vaddr, uint32_t pg_cnt)
{
    
    uint32_t pg_phy_addr;
    uint32_t vaddr = (int32_t)_vaddr, page_cnt = 0;
    ASSERT(pg_cnt >= 1 && vaddr % PG_SIZE == 0);//判断虚拟地址是否合规
    pg_phy_addr = addr_v2p(vaddr);
    ASSERT(pg_phy_addr % PG_SIZE == 0 && pg_phy_addr >= 0x102000);//判断物理地址是否合规
    
    if (pg_phy_addr>=user_pool.phy_addr_start)//如果是用户的物理内存
    {
        
        vaddr -= PG_SIZE;
        while (page_cnt < pg_cnt) {
            vaddr += PG_SIZE;
            pg_phy_addr = addr_v2p(vaddr);

            /* 确保物理地址属于用户物理内存池 */
            ASSERT((pg_phy_addr % PG_SIZE) == 0 &&
                   pg_phy_addr >= user_pool.phy_addr_start);
            /* 先将对应的物理页框归还到内存池 */
            pfree(pg_phy_addr);

            /* 再从页表中清除此虚拟地址所在的页表项pte */
            pagetable_remove(vaddr);

            page_cnt++;
        }
        /* 清空虚拟地址的位图中的相应位 */
        vaddr_remove(pf, _vaddr, pg_cnt);
        printk("mfree_page\n");
    } else {
        
        vaddr -= PG_SIZE;
        while (page_cnt < pg_cnt) {
            vaddr += PG_SIZE;
            pg_phy_addr = addr_v2p(vaddr);
            /* 确保待释放的物理内存只属于内核物理内存池 */
            ASSERT((pg_phy_addr % PG_SIZE) == 0 &&
                   pg_phy_addr >= kernel_pool.phy_addr_start &&
                   pg_phy_addr < user_pool.phy_addr_start);

            /* 先将对应的物理页框归还到内存池 */
            pfree(pg_phy_addr);

            /* 再从页表中清除此虚拟地址所在的页表项pte */
            pagetable_remove(vaddr);

            page_cnt++;
        }
        /* 清空虚拟地址的位图中的相应位 */
        vaddr_remove(pf, _vaddr, pg_cnt);
    }
}
//释放内存
void* sys_free(void *p)
{
    ASSERT(p != NULL);//确保p不是
    enum pool_flags pf;
    struct mem_pool* pool;
    if (running_thread()->pgdir == NULL) {
        ASSERT((uint32_t)p > KERNEL_V_START);//确保是内核的虚拟内存
        pf = PF_KERNEL;
        pool = &kernel_pool;
    } else {
        pf = PF_USER;
        pool = &user_pool;
    }
    lock_acquire(&pool->lock);
    struct mem_block* b = p;
    struct arean* a = block2arena(b);
    ASSERT(a->large == 0 || a->large == 1);
    if (a->large == true && a->desc == NULL) {
        
        mfree_page(pf, a, a->cnt);
    }else
    {
        list_append(&a->desc->freelist, &b->node);
        if (++a->cnt==a->desc->block_per_arena)//如果当前可用内存块数量与该仓库所含有的内存块总数一样则说明整页内存可以回收
        {
            for (uint8_t i = 0; i < a->cnt; i++)
            {
                struct mem_block* bi = arena2block(a, i);
                ASSERT(elem_find(&a->desc->freelist, &bi->node));
                list_remove(&bi->node);
            }
            
            mfree_page(pf, a, 1);
        }
    }
    lock_release(&pool->lock);
}
void* get_a_page_without_opvaddrbitmap(enum pool_flags pf, uint32_t vaddr)
{
    struct mem_pool* pool = pf & PF_KERNEL ? &kernel_pool : &user_pool;//内存池
    lock_acquire(&pool->lock);
    void* page_phyvaddr = palloc(pool);//分配一页物理内存
    if (page_phyvaddr == NULL)//处理内存分配失败的情况
    {
        lock_release(&pool->lock);
        return NULL;
    }
    page_table_add(vaddr, page_phyvaddr);
    lock_release(&pool->lock);
    return (void*)vaddr;
}
void free_a_phy_page(uint32_t pg_phy_addr)
{
    struct mem_pool* pool;
    uint32_t bit_idx;
    if (pg_phy_addr>=user_pool.phy_addr_start)
    {
        pool = &user_pool;
        bit_idx = (pg_phy_addr - user_pool.phy_addr_start) / PG_SIZE;
    } else {
        pool = &kernel_pool;
        bit_idx = (pg_phy_addr - kernel_pool.phy_addr_start) / PG_SIZE;
    }
    bitmap_set(&pool->pool_bitmap, bit_idx, 0);
}
void mem_init() {
    put_str("mem_init start\n");
    lock_init(&kernel_pool.lock);
    lock_init(&user_pool.lock);
    uint32_t mem_bytes_total = (*(
        uint32_t*)(0xb00));  // 将先前loade.s中统计好的内存总量读出并交给变量mem_bytes_total
    mem_pool_init(mem_bytes_total);  // 初始化内存池
    block_desc_init(k_block_descs);
    put_str("mem_init done\n");
}
