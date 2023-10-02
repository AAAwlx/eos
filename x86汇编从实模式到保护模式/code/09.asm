;设置堆桟段指针
mov ax,cs
mov ss,ax
mov sp,0x7c00

;计算gdt逻辑段地址
mov ax,[cs:gdt_base+0x7c00];
mov dx,[cs:gdt_base+0x7c00+0x02]
mov bx,16
div bx
mov ds,ax;商为基地址
mov bx,dx;余数为偏移地址

;创建段描述符表
;创建0#描述符，处理器要求第一个描述符为空
mov dword[bx+0x00],0x00
mov dword[bx+0x04],0x00
;创建1#描述符，代码文本段描述符
mov dword[bx+0x08],0x7c0001ff
mov dword[bx+0x0c],0x00409800  
;创建#2描述符，保护模式下的数据段描述符（文本模式下的显示缓冲区） 
mov dword [bx+0x10],0x8000ffff     
mov dword [bx+0x14],0x0040920b       
;创建3#描述符，堆栈段描述符
mov dword[bx+0x18],0x00007a00
mov dword[bx+0x1c],0x00409600
mov word [cs:gdt_size+0x7c00],31;将gdt_size赋值为31,一共有四个描述符
lgdt [cs:gdt_size+0x7c00]

;开启20A第21根地址线
in al,0x92;读出原有数据
or al,0000_0010B;将标志位设置为1
out 0x92,al;回写打开A20

;准备进入保护模式
cli ;保护模式下的中断机制尚未建立先将中断机制关闭
mov eax,cr0;设置cr0寄存器的pe位
or eax,1
mov cr0,eax
; 进入保护模式
jmp dword 0x0008:flush             ;16位的描述符选择子：32位偏移
                                            ;使用远转移指令清流水线并串行化处理器 
         [bits 32]
flush:
         mov cx,00000000000_10_000B         ;加载数据段选择子(0x10)二进制格式
         mov ds,cx

         ;以下在屏幕上显示"Protect mode OK." 
         mov byte [0x00],'P'  
         mov byte [0x02],'r'
         mov byte [0x04],'o'
         mov byte [0x06],'t'
         mov byte [0x08],'e'
         mov byte [0x0a],'c'
         mov byte [0x0c],'t'
         mov byte [0x0e],' '
         mov byte [0x10],'m'
         mov byte [0x12],'o'
         mov byte [0x14],'d'
         mov byte [0x16],'e'
         mov byte [0x18],' '
         mov byte [0x1a],'O'
         mov byte [0x1c],'K'
mov cx,00000000000_11_000B
mov ss,cx
mov esp,0x7c00
mov ebp,esp
push byte '.'
sub ebp,4
cmp ebp,esp
jnz ghalt
pop eax
mov [0x1e],al
ghalt:     
         hlt                                ;已经禁止中断，将不会被唤醒 
;-------------------------------------------------------------------------------         
gdt_size         dw 0
gdt_base         dd 0x00007e00     ;GDT的物理地址，将gdt的起始地址设置到引导程序之后

;将结尾512字节填充完整            
times 510-($-$$) db 0
                 db 0x55,0xaa