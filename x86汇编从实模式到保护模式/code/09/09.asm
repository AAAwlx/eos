SECTIION header vstart=0;
program_length dd program_end;定义程序长度
code_entry dw start;定义程序起点，偏移地址
           dd section.code.start;基地址
realloc_tbl_len dw (header_end-realloc_begin)/4
realloc_begin:
    code_segment dd section.code.start
    data_segment dd section.datd.start
    stack_segment dd section.stack.start
header_end
;===============================================================================
SECTIION code align=16 vstart=0
new_int_0x70:
;保护寄存器中原有内容
push ax
push bx
push cx
push dx
push es

.w0:
    mov al,0x0a;选择a寄存器
    or al,0x80;屏蔽NMI信号
    out 0x70,al
    in al,0x71
    test al,0x80;测试a寄存器的uip位
    jnz w0.;当时钟处于更新周期时循环等待

;读取rtc中存储的时间
xor al,al
or al,0x80
out 0x70,al
in al,0x71
push ax
;分
mov al,2
or al,0x80
out 0x70,al
in al,0x71
push ax
;时
mov al,4
or al,0x80
out 0x70,al
in al,0x71
push ax
mov al,0x0c
out 0x70,al
in al,0x71
;附加段寄存器指向显示区域
mov ax,0xb800
mov es,ax

;在屏幕上显示时间信息
;小时
pop ax
call bcd_to_ascii 
mov bx,12*160 + 36*2               ;从屏幕上的12行36列开始显示
mov [es:bx],ah
mov [es:bx+2]al
mov al,':'
mov [es:bx+4],al
not byte [es:bx+5];反转显示属性
;分钟
pop ax
call bcd_to_ascii 
mov [es:bx+6],ah
mov [es:bx+8],al
mov al,':'
mov [es:bx+10],al
not byte [es:bx+11];反转显示属性
;秒
pop ax
call bcd_to_ascii 
mov [es:bx+12],ah
mov [es:bx+14],al

;向8259说明中断结束
mov al,0x20
out 0xa0,al;向从片发送
out 0x20,al;向主片发送

pop es
pop dx
pop cx
pop bx
pop ax

iret

bcd_to_ascii:;将bcd转换为ascii函数
mov ah,al
and al,0x0f;将高四位清除
add al,0x30;
shr ah,4
and ah,0x0f
add ah,0x30
ret
;-------------------------------------------------------------------------------
start:;安装中断处理程序

;初始化各个寄存器
mov ax, [stack_segment]
mov ss ,ax
mov sp,ss_pointer
mov ax,[data_segment]
mov ds,ax

;显示初始化信息
mov bx,init_msg
call put_string

;显示安装信息
mov bx,inst_msg
call put_string
mov al,0x70;从片的中断号被初始化为0x70
mov bl,4
mul bl;找到中断向量表中的位置
mov bx,ax
cli;在安装中断处理程序之前关闭中断机制

;开始安装中断处理程序
push es
mov ax,0x0000
mov es,ax
mov dword[es:bx],new_int_0x70;写入偏移地址
mov dword[es:bx+0x02],cs;写入段地址
pop es

mov al,0x0b
or al,0x80;改变端口的最高位不妨碍对寄存器索引的读取，0x70端口的最高位表示是否阻断NMI信号
out 0x70,al
mov al,0x12
out 0x71,al

mov al,0x0c;选择寄存器c，写入同时0x0c的同时也打开了NMI
out 0x70,al
in al,0x71;读取寄存器c，将c寄存器中的内容清零，开启周期性中断

in al,0xa1;通过该端口访问IMR寄存器
and al,0xfe;将IMR寄存器的第0位设置为0,这样就打开了实时时钟与8259芯片的连接
out 0xa1,al
sti;恢复中断

mov bx,done_msg;显示安装完成
call put_string

mov bx,tips_msg;显示提示信息
call put_string

mov cx 0xb800
mov ds,cx
mov cx,0xb800;将数据段寄存器指向显示缓冲区
      mov ds,cx
      mov byte [12*160 + 33*2],'@'       ;屏幕第12行，35列
       
 .idle:
      hlt                                ;使CPU进入低功耗状态，直到用中断唤醒
      not byte [12*160 + 33*2+1]         ;反转显示属性 
      jmp .idle
;-------------------------------------------------------------------------------      
put_string:
    mov cl ,[bx]
    or cl,cl;判断cl是否为0
    jz .exit;句子结束时cl为0
    call put_char
    inc bx
    jmp put_string;循环显示字符
.exit:
    ret
put_char:
    push ax
    push bx
    push cx
    push dx
    push ds
    push es

    ;读取当前光标位置
    mov dx,0x3d4;通过该端口访问显卡
    mov al,0x0e;光标寄存器
    out dx,al
    mov dx,0x3d5
    in al,dx;读取光标寄存器的位置,高8位
    mov ah,al
    mov dx,0x3d4;通过该端口访问显卡
    mov al,0x0f;光标寄存器
    out dx,al
    mov dx,0x3d5
    in al,dx;读取光标寄存器的位置，低8位
    mov bx,ax;将光标的位置放入bx中

    cmp cl,0x0d;对比是不是回车符
    jnz .put_0a;如果不是回车符，看看是不是换行符
    mov ax,bx
    mov bl,80
    div bl
    mul bl
    mov bx,ax
    jmp .set_cursor
.put_0a:
    cmp cl,0x0a;对比是不是换行符
    jnz .put_other;如果不是就按照正常字符读
    add bx,80;
    jmp .roll_screen;如果是换行符加上一行的字符数量对比是否超出屏幕的范围
.put_other:
    mov ax,0xb800
    mov es,ax
    shl bx,1
    mov [es:bx],cl
    shr bx,1
    add bx,1
.roll_screen:
    cmp bx,2000;对比光标是否超出屏幕
    jl .set_cursor;若是超过屏幕则滚屏重置光标
    mov ax,0xb800
    mov ds,ax
    mov es,ax
    cld
    mov si,0xa0
    mov di,0x00
    mov cx,1920
    rep movsw;将后1920个字符向上移动一行
    mov bx,3840
    mov cx,80
    .cls:;循环向最后一行写入黑底白字的空字符
        mov word[es:bx],0x0720
        add
        loop .cls
    mov bx,1920
;重置光标
.set_cursor:
    mov dx,0x34
    mov al,0x0e
    out dx,al
    mov dx,0x35
    mov al,bh
    out dx,al
    mov dx,0x34
    mov al,0x0f
    out dx,al
    mov dx,0x35
    mov al,bl
    out dx,al

    pop es
    pop ds
    pop dx
    pop cx
    pop bx
    pop ax

    ret
;===============================================================================
SECTION data align=16 vstart=0;定义初始化信息

    init_msg       db 'Starting...',0x0d,0x0a,0
                   
    inst_msg       db 'Installing a new interrupt 70H...',0
    
    done_msg       db 'Done.',0x0d,0x0a,0

    tips_msg       db 'Clock is now working.',0
                   
;===============================================================================
SECTIION code align=16 vstart=0;初始化桟的大小
resb 256
ss_pointer:
program_end: