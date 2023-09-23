# bochs安装与使用
## 安装
进入bochs官方网址，选择.tar.gz格式的文件下载，下载好后解压。进入到解压之后的路径中输入以下指令。不建议在命令行中直接安装bochs这样安装好后的程序难以配置。\
要将prefix改写成自己的安装路径。若要想使用gdb调试而不是自带的调试器，则替换enable-x86-debugger为enable-gdb-stub.

>./configure --prefix=/home/ba/bochs\
 --enable-debugger \
 --enable-disasm\
  --enable-iodebug \
  --enable-iodebug\
   --enable-x86-debugger --with-x\
   --with-x11

以上命令若运行成功则会生成makefile文件。\
若是发生报错，则可能需要安装:
>sudo apt install libx11-dev\
sudo apt-get install libxrandr-dev

上述指令执行完成后执行make指令。
>make

make成功之后输入以下指令，完成程序的下载
> make install

## 配置
配置时需创建一个bochs.disk的文件向其中写入如下内容
```
megs : 32
#注意路径 必须是你安装的路径 别弄错了
romimage: file=/usr/local/share/bochs/share/bochs/BIOS-bochs-latest
vgaromimage: file=/usr/local/share/bochs/share/bochs/VGABIOS-lgpl-latest
boot: disk
log: bochs.out
mouse:enabled=0
keyboard:keymap=/usr/local/share/bochs/share/bochs/keymaps/x11-pc-us.map
ata0:enabled=1,ioaddr1=0x1f0,ioaddr2=0x3f0,irq=14
#gdbstub:enabled=1,port=1234,text_base=0,data_base=0,bss_base=0
# 加载磁盘
ata0-master: type=disk, mode=flat, path="./c05_mbr.img", cylinders=58, heads=16,
spt=63
```
## 使用
在使用bochs之前需要使用makefile
```makefile
OBJS_BIN=c05_mbr.bin
.PHONY:
#image: ${OBJS_BIN} 定义了一个名为image的目标,依赖于OBJS_BIN,
#表示要生成一个名为c05.img的磁盘映像文件,并将c05.bin写入该磁盘映像中。
image: ${OBJS_BIN}
#创建一个大小为30MB的全0磁盘映像文件。
	dd if=/dev/zero of=c05_mbr.img bs=512 count=61440
#c05.bin文件写入c05.img磁盘映像文件的第一个扇区。
	dd if=c05_mbr.bin of=c05_mbr.img bs=512 count=1 conv=notrunc
#@-表示忽略删除过程中出现的错误。
#表示将以.asm为扩展名的汇编源文件编译为以.bin为扩展名的二进制文件。
%.bin:%.asm
	nasm $^ -o $@ 
#定义了一个名为run的目标,依赖于OBJS_BIN,表示要运行程序。
run: ${OBJS_BIN}
	make image
	bochs -f bochsrc.disk
clean:
	rm -rf *.img *.out *.lock *.bi
```