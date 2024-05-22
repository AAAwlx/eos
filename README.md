# eos

eos 从 mbr 部分开始实现了一个简单的小型操作系统内核。该系统包含：内存管理、进程管理、文件系统、驱动程序，交互式终端五个部分。

## 快速启动

[关于bochs的配置](https://github.com/AAAwlx/x86-study/blob/main/x86%E6%B1%87%E7%BC%96%E4%BB%8E%E5%AE%9E%E6%A8%A1%E5%BC%8F%E5%88%B0%E4%BF%9D%E6%8A%A4%E6%A8%A1%E5%BC%8F/note/bochs%E5%AE%89%E8%A3%85%E4%B8%8E%E4%BD%BF%E7%94%A8.md)

bochs 配置好后在工作路径中执行如下命令

```c
make all
make run
```

## 内存管理

* 使用 bitmap 作为地址资源的管理方式
* 在虚拟内存的管理中将用户空间与内核空间分隔开
* 使用内存池管理物理内存与虚拟内存资源

## 进程管理

### 线程实现

* 调度采取先进先出的轮训调度方式
* 使用task_pcb结构体记录线程信息

### 系统调用与特权级转换

* 在 TSS 段等硬件支持与虚拟内存的软件支持上，实现了用户进程及其与线程之间的特权级转换
* 在中断门实现系统调用，效仿Linux用0x80号中断作为系统调用入口
* 建立系统调用子功能表syscall_table，利用eax寄存器中的子功能号在该表中索引相应的处理函数
* 用宏实现用户空间系统调用接口syscall，最大支持3个参数的系统调用，通过寄存器传参

### 同步与竞争

* 实现了锁与信号量解决竞争问题

## 文件系统

eos 中实现了简易的 ext2 文件系统。

* 使用inode结构体对文件进行管理
* super block负责保存文件系统元信息的元信息：inode数组的地址及大小、inode位图地址及大小、根目录的地址和大小、空闲块位图的地址和大小

## 驱动程序

* 实现了键盘的驱动程序
* 实现了 ata 磁盘设备的驱动程序
* 通过中断的方式完成与 cpu 的通信
  
## 交互式终端

* 实现了简易的 shell
* 支持 cd ls ps pwd mkdir touch rm 等常用命令
