#include "fork.h"
#include"pipe.h"
#include "bitmap.h"
#include "debug.h"
#include "file.h"
#include "inode.h"
#include "interrupt.h"
#include "memory.h"
#include "process.h"
#include "string.h"
extern void intr_exit(void);//中断返回函数，外部定义

//将父进程的内核空间复制到子目录中
static copy_pcb_vaddrbitmap_stack0(struct task_pcb* child_thread,struct task_pcb*parent_thread)
{
    //直接复制整个pcb块
    memcpy(child_thread, parent_thread, PG_SIZE);
    //单独修改部分信息
    child_thread->parent_pid = fork_pid();
    child_thread->elapsed_ticks = 0;
    child_thread->status = TASK_READY;
    child_thread->ticks = child_thread->priority;//嘀嗒数与优先级相关
    child_thread->parent_pid = parent_thread->parent_pid;
    //将节点从父进程节点中脱链
    child_thread->general_tag.prev = child_thread->general_tag.next = NULL;
    child_thread->all_list_tag.prev = child_thread->all_list_tag.next = NULL;
    block_desc_init(child_thread->u_block_descs);
    //复制父目录中的虚拟地址位图
    uint32_t bitmap_pg_cnt =
      DIV_ROUND_UP((0xc0000000 - USER_VADDR_START) / PG_SIZE / 8, PG_SIZE);
    void* vaddr_btmp = get_kernel_pages(bitmap_pg_cnt);
    if (vaddr_btmp == NULL)
    {
        return -1;
    }
  // 让子进程指向自己的位图
    memcpy(vaddr_btmp, child_thread->userprog_vaddar.vaddr_bitmap.bits,
         bitmap_pg_cnt * PG_SIZE);//将原有的父目录中的内容拷贝到新分配的虚拟地址页中
    child_thread->userprog_vaddar.vaddr_bitmap.bits = vaddr_btmp;//将新的虚拟地址页写入到
    /* 调试用 */
    // ASSERT(strlen(child_thread->name) < 11);
    strcat(child_thread->name, "_fork");
    return 0;
}

/*复制子进程的进程体(代码和数据)及用户栈*/
static void copy_body_stack3(struct task_pcb* child_thread,struct task_pcb* parent_thread,void* buf_page)
{
    uint8_t* vaddr_btmp = parent_thread->userprog_vaddar.vaddr_bitmap.bits;  // 父目录的bitmap
    uint32_t btmp_bytes_len = parent_thread->userprog_vaddar.vaddr_bitmap.bitmap_len;  //位图长度
    uint32_t vaddr_start = parent_thread->userprog_vaddar.vaddr_start;//虚拟内存的起始地

    uint32_t idx_byte = 0;//字节数
    uint32_t idx_bit = 0;//字中的bit位
    uint32_t prog_vaddr = 0;//
    //将被使用过的虚拟地址从父进程复制的子进程中
    while (idx_byte<btmp_bytes_len)
    {
        if (vaddr_btmp[idx_byte])
        {
            idx_bit = 0;
            while (idx_bit<8)
            {
                if ((BITMAP_MASK<<idx_bit)&vaddr_btmp[idx_byte])//如果这位上为1
                {
                    prog_vaddr = (idx_byte * 8 + idx_bit) * PG_SIZE + vaddr_start;//虚拟内存的
                     /* 下面的操作是将父进程用户空间中的数据通过内核空间做中转,最终复制到子进程的用户空间*/
                    memcpy(buf_page, (void*)prog_vaddr, PG_SIZE);//将父进程的内容写入到内核缓冲区中
                    page_dir_activate(child_thread);//激活子进程的虚拟地址
                    get_a_page_without_opvaddrbitmap(PF_USER, prog_vaddr);//为子进程分配内存
                    memcpy( (void*)prog_vaddr, buf_page, PG_SIZE);//将内核缓冲区写入到子进程中
                    page_dir_activate(parent_thread);
                }
            }
            idx_bit++;
        }
        idx_byte++;
    }
}
static int32_t build_child_stack(struct task_pcb* child_thread)
{
    struct intr_stack* intr_0_stack =  (struct intr_stack*)((uint32_t)child_thread + PG_SIZE - sizeof(struct intr_stack));//取出子进程的中断栈
    intr_0_stack->eax = 0;
    //重写thread_stack中
    uint32_t* ret_addr_in_thread_stack = (uint32_t*)intr_0_stack - 1;//自intr_0_stack栈顶-1即就到了thread_stack中的esp中
    uint32_t* esi_ptr_in_thread_stack = (uint32_t*)intr_0_stack - 2;
    uint32_t* edi_ptr_in_thread_stack = (uint32_t*)intr_0_stack - 3;
    uint32_t* ebx_ptr_in_thread_stack = (uint32_t*)intr_0_stack - 4;
    uint32_t* ebp_ptr_in_thread_stack = (uint32_t*)intr_0_stack - 5;

    ret_addr_in_thread_stack = (uint32_t)intr_exit;// 返回地址更新为intr_exit 
    *ebp_ptr_in_thread_stack = *ebx_ptr_in_thread_stack = 0;
    *edi_ptr_in_thread_stack = *esi_ptr_in_thread_stack = 0;
    child_thread->self_kstack = ebp_ptr_in_thread_stack;//更新内核栈
    return 0;
}
//更新inode打开数量
static void update_inode_open_cnts(struct task_pcb* thread)
{
    int32_t local_fd = 3,global_fd = 0;
    while (local_fd<MAX_FILES_OPEN_PER_PROC)
    {
        global_fd = thread->fd_table[local_fd];
        if (global_fd != -1)
        {
            if (is_pipe(local_fd))
            {
                file_table[global_fd].fd_pos++;
            } else {
                file_table[global_fd].fd_inode->i_open_cnts++;
            }
        }
        local_fd++;
    }
}
static int32_t copy_process(struct task_pcb* child_thread,struct task_pcb* parent_thread)
{
    //内核缓冲区
    void* buf_page = get_kernel_pages(1);
    if (buf_page == NULL) {
        return -1;
    }
    if (copy_pcb_vaddrbitmap_stack0(child_thread,parent_thread)==-1)
    {
        return -1;
    }
    child_thread->pgdir = create_page_dir();//为子进程创建页目录项
    if (child_thread->pgdir == NULL) {
        return -1;
    }
    /*复制父进程进程体及用户栈给子进程*/
    copy_body_stack3(child_thread, parent_thread, buf_page);

    /* 构建子进程 thread_stack 和修改返回值 pid*/
    build_child_stack(child_thread);

    /*  更新文件 inode 的打开数 */
    update_inode_open_cnts(child_thread);

    mfree_page(PF_KERNEL, buf_page, 1);
    return 0;
}
pid_t sys_fork(void)
{
    struct task_pcb* parent_thread = running_thread();
    struct task_pcb* child_thread = get_kernel_pages(1);
    if (child_thread == NULL)
    {
        return -1;
    }
    if (copy_process(child_thread, parent_thread) == -1) {
        return -1;
    }
    ASSERT(!elem_find(&general_list, &child_thread->general_tag));
    list_append(&general_list, &child_thread->general_tag);
    ASSERT(!elem_find(&all_list, &child_thread->all_list_tag));
    list_append(&all_list, &child_thread->all_list_tag);

  return child_thread->pid;
}