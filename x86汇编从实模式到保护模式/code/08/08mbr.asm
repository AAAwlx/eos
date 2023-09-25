app_lba_start equ 100           ;声明常数（用户程序起始逻辑扇区号）
SECTION mbr align = 16 vstart=0x7c00;
    ;开头获取用户程序的大小与
    mov ax,0;初始化堆桟段
    mov ss,ax
    mov sp,ax
    ;以下两行从在本段代码中找到用户程序的物理地址
    mov ax,[cs:phy_base]
    mov dx,[cs:phy_base+0x02]
    mov bx,16
    div bx;将物理地址转化为汇编地址
    ;以下两行将es和ds都指向用户程序的起始位置
    mov ds,ax
    mov es,ax
    ;预读用户程序的开头
    xor di, di;将di清零
    mov si, app_lba_start;初始化si使其指向逻辑磁盘的起始位置
    xor bx,bx
    call read_hard_disk_0
    ;将用户程序头部预读出之后判断程序大小
    mov dx,[2];读出用户程序中前两个字获取整个用户程序的大小
    mov ax,[0]
    mov bx,512;
    div bx
    cmp dx,0;对比是否是正好整数个字节
    jnz @1
    dec ax;已经预读了一个区域，需要给ax减少1
    @1:
        cmp ax,0;排除掉所有用户程序占不满一个扇区的情况
        jz direct;若用户程序占不满一个扇区就直接跳过@2
    push ds
    mov cx ,ax;设置循环次数
    @2:
        mov ax,ds
        add ax,0x20
        mov ds,ax;防止字节回绕，每读一个扇区ds增加512字节，汇编地址增加0x20
        xor bx,bx;每次读取前将偏移地址清零
        inc si;使si指向下一个扇区
        call read_hard_disk_0
        loop @2;循环读直至读完
        pop ds 将数据段恢复到指向用户程序头部
    direct:
    mov dx,[0x08];
    mov ax,[0x06];获取用户程序起始位置的段地址
    call calc_segment_base
    mov [0x06],ax;重新填入修改后的代码位置
    mov cx,[0x0a]
    mov bx,0x0c
    realloc 
        mov dx,[bx+0x02]
        mov ax,[bx]
        call calc_segment_base
        mov [bx],ax
        add bx,4;用户段首重定位表一段位置为双字，bx每次加四
        loop realloc
    jmp far [0x04]
    
;————————————————————————————
read_hard_disk_0:
    ;将寄存器中信息入桟保存防止被破坏
    push ax
    push bx
    push cx
    push dx
    ;设置读取扇区的数量
    mov dx,0x1f2;访问该端口
    mov al,1;读取扇区的数量
    out dx,al;向端口写入读取扇区数量
    ;设置扇区号
    inc dx                          ;0x1f3
    mov ax,si
    out dx,al 
    inc dx                          ;0x1f4
    mov ax,ah
    out dx,al 
    inc dx                          ;0x1f5
    mov ax,di
    out dx,al 
    ;通过0x1f7端口发送读取命令，读取磁盘
    inc dx 
    inc dx                          ;0x1f7
    mov al,0x20                     ;读命令
    out dx,al;将端口0x1f7信息输出到al中
    ;判断磁盘工作状态，若在忙，则循环等待
    .waits:
        in al,dx;
        and al,0x88;将除了第七位和第3位全部清零
        cmp al,0x08;若第七位为0,第三位为1,则表明硬盘不忙且已经准备好和主机进行，数据交换
        jnz .waits
    ;读取磁盘数据
    mov cx,256;
    mov dx,0x1f0;硬盘的数据传输接口，通过该端口写入或读取数据
    .readw
        in ax,dx
        mov [bx],ax;基地址为ds,，偏移地址为bx
        add bx,2
        loop .readw
        pop dx
        pop cx
        pop bx
        pop ax
    ret
;————————————————————————————
calc_segment_base: ;计算用户程序中每段的段地址
    push dx
    ;以下两行获取用户程序中每段的物理地址
    add ax,[cs:phy_base]
    adc dx,[cs:phy_base+0x02]
    shr ax,4;右移四位相当于除16获取汇编地址
    ror dx,4;通过循环右移将dx的低四位移动到高四位
    and dx,0fx000;将低12位清理
    or ax,dx;将ax与dx相加合并为16位的新地址
    pop dx
    ret
;_________________________________________________________
phy_base dd 0x10000             ;用户程序被加载的物理起始地址
times 510-($-$$) db 0
                  db 0x55,0xaa;填充到512字节并以aa55结尾