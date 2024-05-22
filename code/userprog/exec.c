#include "exec.h"

#include "elf.h"
#include "file.h"
#include "fs.h"
#include "memory.h"
//#include "pipe.h"
#include "string.h"

extern void intr_exit(void);
#define TASK_NAME_LEN 16
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* 将文件描述符fd指向的文件中,偏移为offset,大小为filesz的段加载到虚拟地址为vaddr的内存 */
static bool segment_load(int32_t fd, uint32_t offset, uint32_t filesz, uint32_t vaddr)
{
    uint32_t vaddr_first_page = vaddr & 0xfffff000;
    uint32_t size_in_first_page = PG_SIZE - (vaddr & 0x00000fff);
    uint32_t occupy_pages = 0;//需要多少页来加载段
    if (filesz>size_in_first_page)//如果当前虚拟页容纳不下该段
    {
        uint32_t left_size = filesz - size_in_first_page;
        occupy_pages = DIV_ROUND_UP(left_size,PG_SIZE)+1;
    }else{
        occupy_pages = 1;
    }
    /* 为进程分配内存 */
    uint32_t page_idx = 0;
    uint32_t vaddr_page = vaddr_first_page;
    while (page_idx<occupy_pages)
    {
        uint32_t* pde = pde_ptr(vaddr_page);//页表指针
        uint32_t* pte = pte_ptr(vaddr_page);//页指针
        //如果该虚拟地址尚未分配物理内存
        if (!(*pde&0x0000001)||!(*pte&0x0000001)) {
            if (get_a_page(PF_USER,vaddr_page)==NULL)
            {
                return false;
            }
        }
        vaddr_page += PG_SIZE;
        page_idx++;
    }

    sys_lseek(fd, offset, SEEK_SET);
    sys_read(fd, (void*)vaddr, filesz);
    return true;
}
static int32_t load(const char* pathname)
{
    int32_t ret = -1;
    //
    struct Elf32_Ehdr elf_header;
    struct Elf32_Phdr prog_header;
    memset(&elf_header, 0, sizeof(struct Elf32_Ehdr));
    int32_t fd = sys_open(pathname, O_RDONLY);
    if (fd == -1)
    {
        return -1;
    }
    if (sys_read(fd,&elf_header,sizeof(struct Elf32_Ehdr)))
    {
        ret = -1;
        goto done;
    }
    /* 校验elf头 */
    if (memcmp(elf_header.e_ident, "\177ELF\1\1\1", 7) ||
        elf_header.e_type != 2 || elf_header.e_machine != 3 ||
        elf_header.e_version != 1 || elf_header.e_phnum > 1024 ||
        elf_header.e_phentsize != sizeof(struct Elf32_Phdr)) {
        ret = -1;
        goto done;
    }
    Elf32_Off prog_header_offset = elf_header.e_phoff;//段首偏移量
    Elf32_Half prog_header_size = elf_header.e_phentsize;//段的大小
    uint32_t prog_idx = 0;
    //遍历表头所有的表项
    while (prog_idx<elf_header.e_phnum) {
        memset(&prog_header, 0, sizeof(struct Elf32_Phdr));
        sys_lseek(fd, prog_header_offset, SEEK_SET);
        if (sys_read(fd,&prog_header,sizeof(struct Elf32_Phdr))!= prog_header_size)//读出段表项
        {
            ret = -1;
            goto done;
        }
        if (prog_header.p_type == PT_LOAD)//如果该段可以加载
        {
            if (!segment_load(fd, prog_header.p_offset, prog_header.p_filesz,
                              prog_header.p_vaddr)) {//将该段加载进内存中
                ret = -1;
                goto done;
            }
        }
        
        prog_header_size += elf_header.e_shentsize;
        prog_idx++;
    }
    ret = elf_header.e_entry;
done:
    sys_close(fd);
    return ret;
}
static void proc_clean() // 清空之前进程的资源
{
    struct task_pcb* cur = running_thread();
    //清空内存块
    for (int i = 0; i < DESC_CNT; i++) {
        list_init(&cur->u_block_descs[i].freelist);
    }
    //清空文件描述符表
    uint8_t fd_idx = 3;
    while (fd_idx < MAX_FILES_OPEN_PER_PROC)
    {
        if (cur->fd_table[fd_idx] != -1)
        {
            sys_close(fd_idx);
        }
        fd_idx++;
    }
}
int32_t sys_execv(const char* path, const char* argv[])
{
    uint32_t argc = 0;
    while (argv[argc])
    {
        argc++;
    }
    struct task_pcb* cur = running_thread();
    proc_clean();
    int32_t entry_point = load(path);
    if (entry_point == -1)
    {
        return -1;
    }
    memcpy(cur->name, path, TASK_NAME_LEN);
    cur->name[TASK_NAME_LEN-1] = 0;
    struct intr_stack* intr_0_stack =
      (struct intr_stack*)((uint32_t)cur + PG_SIZE - sizeof(struct intr_stack));
    /* 参数传递给用户进程 */
    intr_0_stack->ebx = (int32_t)argv;
    intr_0_stack->ecx = argc;
    intr_0_stack->eip = (void*)entry_point;
    /* 使新用户进程的栈地址为最高用户空间地址 */
    intr_0_stack->esp = (void*)0xc0000000;

    /* exec不同于fork,为使新进程更快被执行,直接从中断返回 */
    asm volatile("movl %0, %%esp; jmp intr_exit"
               :
               : "g"(intr_0_stack)
               : "memory");
  return 0;
}