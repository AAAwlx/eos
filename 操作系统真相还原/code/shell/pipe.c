#include "pipe.h"
#include "memory.h"
#include "fs.h"
#include "file.h"
#include "ioqueue.h"
#include "thread.h"
bool is_pipe(uint32_t local_fd)
{
    uint32_t global_fd = fd_local2global(local_fd);
    return file_table[global_fd].fd_flag == PIPE_FLAG;
}
int32_t sys_pipe(int32_t pipefd[2])
{
    int32_t global_fd = get_free_slot_in_global();
    file_table[global_fd].fd_flag = PIPE_FLAG;
    file_table[global_fd].fd_inode = get_kernel_pages(1);
    ioqueue_init((struct ioqueue*)file_table[global_fd].fd_inode);
    if (file_table[global_fd].fd_flag == NULL)
    {
        return -1;
    }
    file_table[global_fd].fd_pos = 2;
    pipefd[0] = pcb_fd_install(global_fd);
    pipefd[1] = pcb_fd_install(global_fd);
    return 0;
}
uint32_t pipe_read(int32_t fd, void* buf, uint32_t count)
{
    char* buffer = buf;
    uint32_t bytes_read = 0;
    uint32_t global_fd = fd_local2global(fd);
    struct ioqueue* ioq = (struct ioqueue*)file_table[global_fd].fd_inode;
    uint32_t ioq_len = ioq_length(ioq);
    uint32_t size = ioq_len > count ? count : ioq_len;//在环形缓冲区尺寸和调用时读写传入缓冲区的尺寸选择较小的尺寸，防止阻塞
    while (bytes_read<size)
    {
        *buffer = ioq_getchar(ioq);
        bytes_read++;
        buffer++;
    }
    return bytes_read;
}
uint32_t pipe_write(int32_t fd, const void* buf, uint32_t count)
{
    uint32_t bytes_write = 0;
    uint32_t global_fd = fd_local2global(fd);
    struct ioqueue* ioq = (struct ioqueue*)file_table[global_fd].fd_inode;
    uint32_t ioq_len = bufsize - ioq_length(ioq);
    uint32_t size = ioq_len > count ? count : ioq_len;//在环形缓冲区尺寸和调用时读写传入缓冲区的尺寸选择较小的尺寸，防止阻塞
    const char* buffer = buf;
    while (bytes_write < size) {
        ioq_putchar(ioq, *buffer);
        bytes_write++;
        buffer++;
    }
    return bytes_write;
}
//实现重定向功能
void sys_fd_redirect(uint32_t old_local_fd, uint32_t new_local_fd)
{
    struct task_pcb* cur = running_thread();
    if (new_local_fd < 3)
    {
        cur->fd_table[old_local_fd] = new_local_fd;
    }else
    {
        uint32_t new_global_fd = cur->fd_table[new_local_fd];
        cur->fd_table[old_local_fd] = new_global_fd;
    }

}