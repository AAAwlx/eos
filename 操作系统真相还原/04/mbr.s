%include "boot.inc"
SECTION MBR vstart=0x7c00
;初始化
mov ax,cs;
mov ds,ax;
mov es,ax;
mov ss,ax;
mov sp,0x7c00
mov ax,0x0800
mov gs,ax;
;启动显示
;输入：
;AH 功能号= 0x06
;AL = 上卷的行数(如果为0,表示全部)
;BH = 上卷行属性
;(CL,CH) = 窗口左上角的(X,Y)位置
;(DL,DH) = 窗口右下角的(X,Y)位置
mov ax,0600h
mov bx,0700h
mov cx,0
mov dx,184fh
int 10h;调用中断号
mov byte [gs:0x00],'l'
mov byte [gs:0x01],0xA4;设置显示属性
mov byte [gs:0x02],'m'
mov byte [gs:0x03],0xA4
mov byte [gs:0x04],'b'
mov byte [gs:0x05],0xA4
mov byte [gs:0x06],'r'
mov byte [gs:0x07],0xA4
;读取磁盘
mov eax,LOADER_START_SECTOR;将磁盘的起始地址传入作为参数
mov bx,LOADER_BASE_ADDR;将加载的目的地址作为参数传入
mov cx,4;将要读取的总的扇区数填入
call read_disk

;跳转执行
jmp LOADER_BASE_ADDR;

read_disk:
mov esi,eax
mov di,cx
mov dx,0x1f2;
mov al,cl;要读取的扇区数
out dx,al
;将物理磁盘起始的28位置写入端口
mov eax,esi;恢复eax
inc dx;0x1f3
out dx,al
mov cl,8
inc dx;0x1f4
shr eax,cl
out dx,al
inc dx;0x1f5
shr eax,cl
out dx,al
inc dx;0x1f6
shr eax,cl
and al,0x0f;将最低四位保留
or al,0xe0;设置lab模式，磁盘分chs与lab两种模式
inc dx;0x1f7
mov al,0x20;写入读模式
out dx,al
.wait:
    in al,dx;从端口读出目前的状态
    and al,0x88
    cmp al,0x08
    jnz .wait;如果没准备好，就循环等待
mov ax,di
mov dx,256
mul dx
mov cx,ax
mov dx,0x1f0
.read:
    in ax,dx
    mov [bx],ax
    add bx,2
    loop .read
ret
times (510-($-$$)) db 0
db 0x55,0xaa
;跳转执行