#include "wait_exit.h"

#include "debug.h"
#include "file.h"
#include "fs.h"
#include "list.h"
#include "pipe.h"
/* 释放用户进程资源:
 * 1 页表中对应的物理页
 * 2 虚拟内存池占物理页框
 * 3 关闭打开的文件 */
static void realease_prog_resource(struct task_pcb* release_thread)
{
    uint32_t* pgdir_vaddr = release_thread->pgdir;

    uint16_t user_pde_nr = 768, pde_idx = 0;//低于3GB页目录对应前768项
    uint32_t pde = 0;
    uint32_t* v_pde_ptr = NULL;

    uint16_t user_pte_nr = 1024, pte_idx = 0;
    uint32_t pte = 0;
    uint32_t* v_pte_ptr = NULL;

    uint32_t* first_pte_vaddr_in_pde = NULL;

    uint32_t pg_phy_addr = 0;
    //释放
    while (pde_idx<user_pde_nr)
    {
        v_pde_ptr = pgdir_vaddr + pde_idx;
        pde = *v_pde_ptr;
        if (pde&0x00000001)//该页目录项是否存在
        {
            first_pte_vaddr_in_pde = pte_ptr(pde_idx * 0x400000);
            pte_idx = 0;
            while (pte_idx < user_pte_nr) {//该页表项是否存在
                v_pte_ptr = first_pte_vaddr_in_pde + pte_idx;
                pte = *v_pte_ptr;
                if (pte & 0x00000001) {
                    // 在位图中清零
                    pg_phy_addr = pte & 0xfffff000;
                    free_a_phy_page(pg_phy_addr);
                }
                pte_idx++;
            }   
            pg_phy_addr = pde & 0xfffff000;//获取页目录项中的物理地址
            free_a_phy_page(pg_phy_addr);
        }
        pde_idx++;
    }
    /* 回收用户虚拟地址池所占的物理内存*/
    uint32_t bitmap_pg_cnt =(release_thread->userprog_vaddar.vaddr_bitmap.bitmap_len) / PG_SIZE;
    uint8_t* user_vaddr_pool_bitmap = release_thread->userprog_vaddar.vaddr_bitmap.bits;
    mfree_page(PF_KERNEL, user_vaddr_pool_bitmap, bitmap_pg_cnt);
    //关闭打开的文件
    uint32_t fd_idx = 3;
    while (fd_idx<MAX_FILES_OPEN_PER_PROC) {
        if (release_thread->fd_table[fd_idx]!=0)
        {
            if (is_pipe(fd_idx))
            {
                uint32_t global_fd = fd_local2global(fd_idx);
                if (--file_table[global_fd].fd_pos == 0)//如果两个通道都关闭
                {
                    mfree_page(PF_KERNEL, file_table->fd_inode, 1);
                    file_table[global_fd].fd_inode == NULL;
                }
            } else {
                sys_close(fd_idx);
            }
        }
        fd_idx++;
    }
}
/* list_traversal 的回调函数,
 * 查找 pelem 的 parent_pid 是否是 ppid,成功返回 true,失败则返回 false */
static bool find_child(struct list_node* pelem, int32_t ppid) 
{
    struct task_pcb* pthread =
      elem2entry(struct task_pcb, all_list_tag, pelem);
    if (pthread->parent_pid == ppid)
    {
        return true;
    }
    return false;
}
/* list_traversal 的回调函数,
 * 查找状态为 TASK_HANGING 的任务 */
static bool find_hanging_child(struct list_node* pelem, int32_t ppid)
{
    struct task_pcb* pthread =
      elem2entry(struct task_pcb, all_list_tag, pelem);
    if (pthread->status == TASK_HANGING&&pthread->parent_pid == ppid)
    {
        return true;
    }
    return false;
}
static bool init_adopt_a_child(struct list_node* pelem, int32_t pid) {
  struct task_pcb* pthread =
      elem2entry(struct task_pcb, all_list_tag, pelem);
  if (pthread->parent_pid == pid) {
    pthread->parent_pid = 1;
    return true;
  }
  return false;
}
/* 等待子进程调用 exit,将子进程的退出状态保存到 status 指向的变量.
 * 成功则返回子进程的 pid,失败则返回−1 */
pid_t sys_wait(int32_t* status)
{
    struct task_pcb* parent_thread = running_thread();
    while (1)
    {
        struct list_node* child =
            list_traversal(&all_list, find_hanging_child, parent_thread->pid);
        //如果有挂起的子进程
        if (child!=NULL)
        {
            struct task_pcb* pthread = elem2entry(struct task_pcb, all_list_tag, child);
            *status = pthread->exit_status;//子进程返回值
            uint16_t child_pid = pthread->pid;
            thread_exit(pthread, false);//在父进程中让子进程删除在队列中的内容后退出
            return child_pid;
        }
        child = list_traversal(&all_list, find_child, parent_thread->pid);
        //如果已经没有子进程则返回错误
        if (child==NULL)
        {
            return -1;
        }else//如果有进程则令父进程等待子进程将自己阻塞并设置为挂起状态
        {
            thread_lock(TASK_WAITING);
        }
    }
}
void sys_exit(int32_t status)
{
    struct task_pcb* child_thread = running_thread();
    child_thread->exit_status = status;//子进程给父进程返回的结果
    if (child_thread->parent_pid == -1)
    {
        PANIC("sys_exit: child_thread->parent_pid is -1\n");
    }
    list_traversal(&all_list, init_adopt_a_child, child_thread->pid);//找到待退出子进程的子进程并将他们转交给init进程
    realease_prog_resource(child_thread);//回收子进程资源
    struct task_pcb* parent_thread = pid2thread(child_thread->parent_pid);
    if (parent_thread->status == TASK_WAITING) {//唤醒父进程
        thread_unlock(parent_thread);
    }
    thread_lock(TASK_HANGING);//将自己阻塞并设置为挂起状态
}