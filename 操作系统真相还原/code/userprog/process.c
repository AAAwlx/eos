#include"printk.h"
#include"../kernel/memory.h"
#include"string.h"
#include"bitmap.h"
#include"../thread/thread.h"
#include"process.h"
#include"global.h"
#include"interrupt.h"
#include"tss.h"
#include"debug.h"
void start_process(void* filename_)
{
    void* function = filename_;
    struct task_pcb* pthread = running_thread();
    pthread->self_kstack += sizeof(struct thread_stack);//将栈指针移动到中断栈的位置
    struct intr_stack* proc_stack = pthread->self_kstack;
    proc_stack->edi = proc_stack->esi = proc_stack->ebp = proc_stack->esp_dummy = 0;
    proc_stack->ebx = proc_stack->edx = proc_stack->ecx = proc_stack->eax = 0;
    proc_stack->gs = 0;		 // 不太允许用户态直接访问显存资源,用户态用不上,直接初始为0
    proc_stack->ds = proc_stack->es = proc_stack->fs = SELECTOR_U_DATA;
    proc_stack->eip = function;
    proc_stack->esp = (void*)((uint32_t)get_a_page(PF_USER, USER_STACK3_VADDR) + PG_SIZE);
    proc_stack->cs = SELECTOR_U_CODE;
    proc_stack->ss = SELECTOR_U_STACK;
    asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (proc_stack) : "memory");
}
void page_dir_activate(struct task_pcb* p_thread)
{
     uint32_t pagedir_phy_addr = 0x100000;  // 需要重新填充页目录
  if (p_thread->pgdir != NULL) {         // 用户进程有自己的也目录表
    pagedir_phy_addr = addr_v2p((uint32_t)p_thread->pgdir);
  }
  // 更新cr3,页表生效
  asm volatile("movl %0,%%cr3" ::"r"(pagedir_phy_addr) : "memory");
}
void process_activate(struct task_pcb* p_thread)
{
    ASSERT(p_thread != NULL);
    /* 激活该进程或线程的页表 */
    page_dir_activate(p_thread);
    if (p_thread->pgdir)
    {
        update_tss_esp(p_thread);
    }
}
uint32_t* create_page_dir(void)
{
    uint32_t* page_dir_vaddr = get_kernel_pages(1);
    if (page_dir_vaddr=NULL)
    {
        console_put_str("create_page_dir: get_kernel_page failed!");
        return NULL;
    }
    memcpy((uint32_t*)(page_dir_vaddr + 0x300 * 4),
           (uint32_t*)(0xfffff000 + 0x300 * 4),1024);//将映射内核态的后1gb虚拟地址复制到新的页目录中
    uint32_t new_page_dir_phy_addr = addr_v2p(page_dir_vaddr);//获得页目录的物理地址
    page_dir_vaddr[1023] = new_page_dir_phy_addr| PG_US_U | PG_RW_W | PG_P_1;//在页尾填入自己的地址
}
//初始化用户状态下的进程虚拟内存池的位图
void create_user_vaddr_bitmap(struct task_pcb* user_prog)
{
    user_prog->userprog_vaddar.vaddr_start=USER_VADDR_START;
    uint32_t bitmap_pg_cnt = DIV_ROUND_UP((0xc0000000 - USER_VADDR_START) / PG_SIZE / 8 , PG_SIZE);
    user_prog->userprog_vaddar.vaddr_bitmap.bits = get_kernel_pages(bitmap_pg_cnt);
    user_prog->userprog_vaddar.vaddr_bitmap.bitmap_len =
        (0xc0000000 - USER_VADDR_START) / 8 / PG_SIZE;
    bitmap_init(&user_prog->userprog_vaddar.vaddr_bitmap);
}
void process_execute(void* filename, char* name)
{
    struct task_pcb* pthread = get_kernel_pages(1);
    init_thread(pthread, name, default_prio);
    //printk("stack_magic: %s: %x\n", pthread->name, pthread->stack_magic);
    create_user_vaddr_bitmap(pthread);
    //printk("%s: %x\n", name, filename);
    thread_create(pthread, start_process, filename);
    //printk("%s: %x\n", pthread->name, pthread->stack_magic);
    pthread->pgdir = create_page_dir();
    block_desc_init(pthread->u_block_descs);
    enum intr_status old = intr_disable();
    
    //将进程加入到就绪队列和全部队列中
    ASSERT(!elem_find(&general_list, &pthread->general_tag));
    list_append(&general_list, &pthread->general_tag);
    ASSERT(!elem_find(&all_list, &pthread->all_list_tag));
    list_append(&all_list, &pthread->all_list_tag);
    intr_set_status(old);
}