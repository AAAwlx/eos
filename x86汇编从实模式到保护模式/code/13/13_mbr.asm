core_base_address equ 0x0004000;内核被加载的内存地址
core_start_sector equ 0x0000001;内核程序所在的逻辑扇区

mov ax,cs 
mov ss,ax
mov sp,0x7c00

mov eax, [cs:pgdt+0x7c00+0x02];取得gdt的物理地址
xor edx,edx
mov ebx,16;将逻辑地址转化为物理地址
div ebx

;安装最基础的数据段，代码段，堆栈段，附加显示段等最基础的段描述符
mov ds,eax;商为基地址
mov ebx,edx;余数为段内偏移地址
;数据段描述符
mov dword [ebx+0x08],0x0000ffff;基地址为0,界限为0xffff
mov dword [ebx+0x0c],0x00cf9200;粒度为4k全局寻址范围
;代码段描述符
mov dword [ebx+0x10],0x7c0001ff;基地址为0
mov dword [ebx+0x14],0x00409800;粒度为4k全局寻址范围
;堆栈描述符
mov dword [ebx+0x18],0x7c00fffe;
mov dword [ebx+0x1c],0x00cf9600
;建立保护模式下的显示缓冲区描述符   
mov dword [ebx+0x20],0x80007fff    ;基地址为0x000B8000，界限0x07FFF 
mov dword [ebx+0x24],0x0040920b    ;粒度为字节
mov word [cs:pgdt+0x7c00],39;将gdt的界限填入
lgdt [cs:pgdt+0x7c00]
in al,0x92
or al,0000_0010B;设置第一位为1,打开A20
out 0x92,al;回写

cli;禁止中断

mov eax,cr0
or eax,1
mov cr0,eax
jmp dword 0x0010:flush
[bits 32]
flush:
;初始化各个段选择器
mov eax,0x0008;加载段选择子
mov ds,eax
mov eax,0x018;
mov ss,eax
xor esp,esp
mov edi,core_base_address
mov eax,core_start_sector
mov ebx,edi
call read_hard_disk_0
mov eax,[edi];从内核的头部找到整个内核程序的大小
xor edx,edx
mov ecx,512
div ecx
or edx,edx;是否有余数，如果有向上舍入跳过减去预读扇区数的步骤
jnz @1
dec eax
@1:
    or eax,eax;如果不满一个扇区直接跳到重定位的代码段执行
    jz setup
    mov ecx,eax
    mov eax,core_start_sector
    inc eax
@2:
    call read_hard_disk_0
    inc eax
    loop @2
setup:
mov esi,[0x7c00+pgdt+0x02];不使用cs前缀寻址，默认ds全局可以访问
;装配公共例程段
mov eax,[edi+0x04];从内核程序头部获得公共例程段的汇编地址
mov ebx,[edi+0x08]
sub ebx,eax;前一段减去后一段等于段界限
dec ebx
add eax,edi;将内核程序头部段的地址与加载内核程序的基地址相加得到公共例程段的段基地址
mov ecx,0x00409800
call make_gdt_descriptor
mov [esi+0x28],eax
mov [esi+0x2c],edx
;装配核心数据段
mov eax,[edi+0x08]
mov ebx,[edi+0x0c]
sub ebx,eax
dec ebx
add eax+edi
mov ecx,0x00409200
call make_gdt_descriptor
mov [esi+0x30],eax
mov,[esi+0x34],edx
;建立核心代码段描述符
mov eax,[edi+0x0c]                 ;核心代码段起始汇编地址
mov ebx,[edi+0x00]                 ;程序总长度
sub ebx,eax
dec ebx                            ;核心代码段界限
add eax,edi                        ;核心代码段基地址
mov ecx,0x00409800                 ;字节粒度的代码段描述符
call make_gdt_descriptor
mov [esi+0x38],eax
mov [esi+0x3c],edx
mov word [0x7c00+pgdt],63
lgdt [0x7c00+pgdt]
jmp far [edi+0x10]
;-------------------------------------------------------------------------------
make_gdt_descriptor:
mov edx,eax
shl eax,16
or ax,bx
and edx,0xffff0000;将前16位清空
rol edx 8;循环右移将高8位移动到低八位
bswap edx;将高八位和低八位互换
xor bx,bx;将低16位清零，将中间的4位装配。
or edx,bx;装配段界限的高四位
or edx,ecx;装配属性
ret
;-------------------------------------------------------------------------------
read_hard_disk_0:                        ;从硬盘读取一个逻辑扇区
    push eax                                     ;EAX=逻辑扇区号
    push ebx                                     ;返回：EBX=EBX+512 
    push ecx
    push edx                                     ;DS:EBX=目标缓冲区地址
    ;将28位磁盘扇区号逐个写入
    push eax
    mov dx,0x12
    mov al,1
    out dx,al

    inc dx
    pop eax
    out dx,al 

    inc dx
    mov cl,8
    shr eax,cl
    out dx,al      

    inc dx
    shr eax,cl
    or al,0xe0
    out dx,al
    ;向磁盘发出读命令
    inc dx
    mov al,0x20
    out dx,al  
.waits:
    in al,dx
    and al,0x88
    cmp al,0x08
    jnz .waits;若是忙，或硬盘没有准备好读取则循环等待
    mov ecx,256
    mov dx,0x1f0
.read
    in ax,dx
    mov [ebx],ax
    add ebx,2
    loop .read
    pop edx
    pop ecx
    pop eax

    ret
;-------------------------------------------------------------------------------
pgdt        dw 0
            dd 0x00007e00;GDT的物理地址
 ;-------------------------------------------------------------------------------
 times 510-($-$$) db 00
                  db 0x55 0xaa   