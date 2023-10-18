;定义常数，段选择子
        core_code_seg_sel     equ  0x38    ;内核代码段选择子
        core_data_seg_sel     equ  0x30    ;内核数据段选择子 
        sys_routine_seg_sel   equ  0x28    ;系统公共例程代码段的选择子 
        video_ram_seg_sel     equ  0x20    ;视频显示缓冲区的段选择子
        core_stack_seg_sel    equ  0x18    ;内核堆栈段选择子
        mem_0_4_gb_seg_sel    equ  0x08    ;整个0-4GB内存的段的选择子
;-------------------------------------------------------------------------------
;核心程序头部，定义各个段的地址以及程序长度
    core_length      dd core_end       ;核心程序总长度#00

    sys_routine_seg  dd section.sys_routine.start
                                            ;系统公用例程段位置#04

    core_data_seg    dd section.core_data.start
                                            ;核心数据段位置#08

    core_code_seg    dd section.core_code.start
                                            ;核心代码段位置#0c


    core_entry       dd start          ;核心代码段入口点#10
                     dw core_code_seg_sel
;===============================================================================
SECTION core_data vstart = 0;内核数据段
;gdt的地址和界限
    pgdt dw 0
         dd 0
;内存分配的起始地址
    ram_alloc dd 0x00100000
    
;符号表（系统调用），每条符号表中包含256个字节的符号调用名，段内偏移地址，段描述符选择子
    salt:
    salt_1    db '@PrintString'
                  times 256-($-salt_1) db 0
              dd put_string
              dw sys_routine_seg_sel
    salt_2    db '@ReadDiskData'
                  times 256($-salt_2) db 0
              dd read_head_disk_0
              dw sys_routine_seg_sel
    salt_3    db  '@PrintDwordAsHexString'
                   times 256-($-salt_3) db 0
              dd  put_hex_dword
              dw  sys_routine_seg_sel

    salt_4    db  '@TerminateProgram'
                   times 256-($-salt_4) db 0
              dd  return_point
              dw  core_code_seg_sel
    salt_item_len equ $-salt_4;一条符号表的大小
    salt_items    equ ($-salt)/salt_item_len;一共有多少条符号表被定义
;字符信息
        message_1        db  '  If you seen this message,that means we '
                          db  'are now in protect mode,and the system '
                          db  'core is loaded,and the video display '
                          db  'routine works perfectly.',0x0d,0x0a,0

        message_2        db  '  System wide CALL-GATE mounted.',0x0d,0x0a,0
         
        message_3        db  0x0d,0x0a,'  Loading user program...',0
         
        do_status        db  'Done.',0x0d,0x0a,0
         
        message_6        db  0x0d,0x0a,0x0d,0x0a,0x0d,0x0a
                          db  '  User program terminated,control returned.',0

        bin_hex          db '0123456789ABCDEF';put_hex_dword子过程用的查找表 
        core_buffer times 2048 db 0;内核缓冲区
        ;cpu信息
        cpu_brnd0        db 0x0d,0x0a,'  ',0
        cpu_brand  times 52 db 0
        cpu_brnd1        db 0x0d,0x0a,0x0d,0x0a,0
        ;tcb调用链的头指针
        tcb_chain        dd 0
core_data_end:
;===============================================================================
SECTION sys_routine vstart = 0;共用例程api
;显示字符串
put_string:                                 ;显示0终止的字符串并移动光标 
                                            ;输入：DS:EBX=串地址
         push ecx
  .getc:
         mov cl,[ebx]
         or cl,cl
         jz .exit
         call put_char
         inc ebx
         jmp .getc

  .exit:
         pop ecx
         retf                               ;段间返回

;-------------------------------------------------------------------------------
put_char:                                   ;在当前光标处显示一个字符,并推进
                                            ;光标。仅用于段内调用 
                                            ;输入：CL=字符ASCII码 
          ;显示0终止的字符串并移动光标 
                                            ;输入：DS:EBX=串地址
         push ecx
  .getc:
         mov cl,[ebx]
         or cl,cl
         jz .exit
         call put_char
         inc ebx
         jmp .getc
  .exit:
         pop ecx
         retf                               ;段间返回

         pushad

         ;以下取当前光标位置
         mov dx,0x3d4ebx
         mov al,0x0e
         out dx,al
         inc dx                             ;0x3d5
         in al,dx                           ;高字
         mov ah,al

         dec dx                             ;0x3d4
         mov al,0x0f
         out dx,al
         inc dx                             ;0x3d5
         in al,dx                           ;低字
         mov bx,ax                          ;BX=代表光标位置的16位数

         cmp cl,0x0d                        ;回车符？
         jnz .put_0a
         mov ax,bx
         mov bl,80
         div bl
         mul bl
         mov bx,ax
         jmp .set_cursor

  .put_0a:
         cmp cl,0x0a                        ;换行符？
         jnz .put_other
         add bx,80
         jmp .roll_screen

  .put_other:                               ;正常显示字符
         push es
         mov eax,video_ram_seg_sel          ;0xb8000段的选择子
         mov es,eax
         shl bx,1
         mov [es:bx],cl
         pop es

         ;以下将光标位置推进一个字符
         shr bx,1
         inc bx

  .roll_screen:
         cmp bx,2000                        ;光标超出屏幕？滚屏
         jl .set_cursor

         push ds
         push es
         mov eax,video_ram_seg_sel
         mov ds,eax
         mov es,eax
         cld
         mov esi,0xa0                       ;小心！32位模式下movsb/w/d 
         mov edi,0x00                       ;使用的是esi/edi/ecx 
         mov ecx,1920
         rep movsd
         mov bx,3840                        ;清除屏幕最底一行
         mov ecx,80                         ;32位程序应该使用ECX
  .cls:
         mov word[es:bx],0x0720
         add bx,2
         loop .cls

         pop es
         pop ds

         mov bx,1920

  .set_cursor:
         mov dx,0x3d4
         mov al,0x0e
         out dx,al
         inc dx                             ;0x3d5
         mov al,bh
         out dx,al
         dec dx                             ;0x3d4
         mov al,0x0f
         out dx,al
         inc dx                             ;0x3d5
         mov al,bl
         out dx,al

         popad
         
         ret                                

;-------------------------------------------------------------------------------
read_hard_disk_0:
    push eax
    push ecx
    push edx

    push eax

    mov dx,0xf12
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
    out dx,al

    inc dx
    shr eax,cl
    or al,0x0e
    out dx,al

    inc dx
    mov al,0x20
    out dx,al;读命令

  .waits:
        in al,dx
        and al,0x88
        cmp al,0x08
        jnz .waits
  .readw:
    in ax,dx
    mov [ecx],ax
    add ebx,2
    loop .readw
    pop edx
    pop ecx
    pop eax
      
    retf

;-------------------------------------------------------------------------------
;安装描述符
set_up_gdt_descriptor:
    push eax
    push ebx
    push edx
    push ds
    push es
    mov ebx,core_data_seg_sel
    mov ds,ebx

    sgdt,[pgdt];将取出gdtr寄存器的内容存入数据段中pgat预留的地方
    mov ebx,mem_0_4_gb_seg_sel
    mov es,ebx
    movzx ebx,[pgdt];将16位的gdt界限扩展成32位便于计算
    inc bx;计算机启动时段界限为0xffff，若在ebx上直接加1，则为0x00010000,若在bx上加中则，反转为0
    add ebx,[pgdt+0x02];gdt表的位置加上段界限就是目前gdt表装配的地址
    
    ;将新的段描述符内容装配到gdt表之中
    mov [es:ebx],eax
    mov [es:ebx+4],edx
    add word [pgdt],8;gdt的界限值加8
    
    ;更新gdtr寄存器中的内容
    lgdt [pgdt]

    ;拼凑索引号
    mov ax,[pgdt]
    xor dx,dx
    mov bx,8
    div bx
    mov cx,ax
    shl cx,3

    pop es
    pop ds
    pop edx
    pop ebx
    pop eax

    retf
;-------------------------------------------------------------------------------
;凑段描述符
make_seg_descriptor:
    mov edx,eax
    shl eax,16;将段内偏移量向右移动16位
    or ax,bx;将段界限装配到eax中的
    ;高32位装配完成
    and edx,0xffff0000;清理低16位
    rol edx 8;循环右移最高8位将最低位移动到
    bswap edx;前后八位交换位置
    xor bx,bx
    or edx,ebx
    or edx,ecx;装配属性

    retf
;-------------------------------------------------------------------------------
;凑门描述符
make_gate_descriptor:
    push ebx
    push ecx
    mov edx,eax
    and edx,0xffff0000
    or dx,cx
    and 0x0000ffff
    shl eax,16
    or eax,ebx
    pop ecx
    pop ebx
    retf
;-------------------------------------------------------------------------------
;分配内存地址
allocate_memory:
    push ds
    push eax
    push ebx

    mov eax,core_data_seg_sel
    mov ds,eax

    mov eax,[ram_alloc]
    add eax,ecx;

    mov ecx,[ram_alloc];将分配的起始地址写入
    mov ebx,eax
    and ebx,0xfffffffc;将最后三位变成0
    add ebx,4;加4和4对齐，向上舍入
    test eax,0x00000003;测试是不是4的倍数
    cmovnz eax,ebx;若eax是4的倍数就直接使用eax,若不是就使用ebx中向上舍入的内存大小
    mov [ram_alloc],eax
    pop ebx
    pop eax
    pop ds
    retf
sys_routine_end:
;===============================================================================
SECTION core_code vstart = 0;内核代码段
;安装LDT描述符
fill_descriptor_in_ldt:
    push eax
    push edx
    push edi
    push ds
    ;获取LDT的基地址
    mov ecx,mem_0_4_gb_seg_sel
    mov ds,ecx
    mov edi,[es:ebx+0x0c];将LDT的基地址取出来
    
    xor ecx,ecx
    mov cx,[es:ebx+0x0a];获得LDT的界限值
    inc cx
    ;安装描述符
    mov [edi+ecx+0x00],eax
    mov [edi+ecx+0x04],edx
    dec cx
    add cx,8
    mov [es:ebx+0x0a],cx;将新的界限值重新填入
    ;拼凑LDT选择子
    mov ax,cx
    xor dx,dx
    mov bx,8
    div bx
    mov cx,ax
    shl cx
    or cx,0000_0000_0000_0100B         ;使TI位=1，指向LDT，最后使RPL=00 
    pop ds
    pop edi
    pop edx
    pop eax
;装载用户程序
load_relocate_program: 
    pushad

    push es
    push ds

    mov ebp,esp 
    mov eax,mem_0_4_gb_seg_sel;将全局段选择子放入es附加段中
    mov es,eax
    mov esi,[ebp+11*4];将之前压入桟中的tcb块的地址读取出来放入esi中，ebp默认基地址为ess

    mov ecx,160
    call sys_routine_seg_sel:allocate_memory;为LDT分配地址
    mov [es:esi+0x0c],ecx;将新分配的内存地址放入tcb的块中，LDT地址
    mov word[es:esi+0x0a],0xffff;初始化
    
    ;加载用户程序
    mov eax,core_data_seg_sel
    mov ds,eax;将数据段切换到内核数据段

    mov eax,[ebp+12*4];从桟中取出之前压入的逻辑扇区号
    mov ebx,[core_buffer];将内核缓冲区的位置当作读取的目的地址传入read_hard_disk_0中
    call sys_routine_seg_sel:read_hard_disk_0

    ;判断整个程序有多大
    mov eax,[core_buffer];从用户程序头部读出头部大小
    mov ebx,eax
    and ebx,0xfffffe00
    add ebx,512
    test eax,0x000001ff;对比看eax是否是512的倍数
    cmovnz eax,ebx
    
    ;分配内存
    mov ecx,eax;传参
    call sys_routine_seg_sel:allocate_memory
    mov [es:esi+0x06],ecx;将用户程序的地址填入tbc块之中
    
    ;计算有几个逻辑块
    mov ebx,ecx;将分配好的地址填入ebx寄存器中等待传参入读取函数中
    xor edx,edx
    mov ecx,512
    div ecx
    mov ecx,eax;将总扇区数放入计数器中
    mov eax,mem_0_4_gb_seg_sel;将数据段设置为全局描述符
    mov ds,eax
    mov eax,[ebp+12*4];将桟中的磁盘号取出

  .b1:
    call sys_routine_seg_sel:read_head_disk_0
    inc eax
    loop .b1

    ;将头部段安装到LDT中
    mov edi,[es:esi+0x06];获得程序加载的地址
    mov eax,edi;获取程序的基地址
    mov ebx,[es:edi+0x04];获得头部段的长度
    dec ebx
    mov ecx,0x0040f200;设置段描述符的属性
    call sys_routine_seg_sel:make_seg_descriptor;拼凑段描述符
    mov ebx,esi
    call fill_descriptor_in_ldt;ebx中存放该任务tcb块的地址作为参数，返回值为选择子
    or cx,0000_0000_0000_0011B;设置选择子的特权级别为3，RTL
    mov [edi+0x04],cx;将选择子回填回用户程序开头处
    mov [es:esi+0x44],cx;将选择子放入tcb块中
    ;将代码段安装到LDT中
    mov eax,[edi+0x14]
    mov ebx,[edi+0x18]
    dec ebx
    mov ecx,0x0040f800
    call sys_routine_seg_sel:make_seg_descriptor 
    mov ebx,esi
    call fill_descriptor_in_ldt
    or cx,0000_0000_0000_0011B
    mov [edi+0x14],cx
    ;建立程序数据段描述符
    mov eax,edi
    add eax,[edi+0x1c]                 ;数据段起始线性地址
    mov ebx,[edi+0x20]                 ;段长度
    dec ebx                            ;段界限 
    mov ecx,0x0040f200                 ;字节粒度的数据段描述符，特权级3
    call sys_routine_seg_sel:make_seg_descriptor
    mov ebx,esi                        ;TCB的基地址
    call fill_descriptor_in_ldt
    or cx,0000_0000_0000_0011B         ;设置选择子的特权级为3
    mov [edi+0x1c],cx                  ;登记数据段选择子到头部

         ;建立程序堆栈段描述符
         mov ecx,[edi+0x0c]                 ;4KB的倍率 
         mov ebx,0x000fffff
         sub ebx,ecx                        ;得到段界限
         mov eax,4096                        
         mul ecx                         
         mov ecx,eax                        ;准备为堆栈分配内存 
         call sys_routine_seg_sel:allocate_memory
         add eax,ecx                        ;得到堆栈的高端物理地址 
         mov ecx,0x00c0f600                 ;字节粒度的堆栈段描述符，特权级3
         call sys_routine_seg_sel:make_seg_descriptor
         mov ebx,esi                        ;TCB的基地址
         call fill_descriptor_in_ldt
         or cx,0000_0000_0000_0011B         ;设置选择子的特权级为3
         mov [edi+0x08],cx                  ;登记堆栈段选择子到头部

    ;重定位salt
    mov eax,mem_0_4_gb_seg_sel
    mov es,eax
    mov eax,core_data_seg_sel
    mov ds,eax
    cld
    mov ecx,[es:edi+0x24];获取u-salt的数量
    add edi,0x28;
    .b2:
        push ecx
        push edi
        mov ecx,salt_items
        mov esi,salt
        .b3:
            push edi
            push esi
            push ecx
            mov ecx,64
            repe cmpsd
            jnz .b4
            mov eax,[esi];从内核代码段中取出段内偏移地址
            mov [es:edi-256],eax;将内核段内偏移地址填入用户代码段，代替调用符号
            mov ax,[esi+4];读出段选择子
            or ax,0000_0000_0000_0011B;将段选择子由内核权限降至用户权限
            mov [es:di-252],ax;回填到用户程序
        .b4
            pop ecx
            pop esi
            add esi,salt_item_len
            pop edi
            add edi
            loop .b3
        pop edi
        add edi,256
        pop ecx
        loop .b2 
    
    ;创建1级特权堆栈   
    mov ecx,[ebp+11*4]
    mov ecx,4096
    mov eax,ecx
    mov [es:esi+0x1a],ecx
    shl dword[es:esi+0x1a],12
    call sys_routine_seg_sel:allocate_memory
    add eax,ecx;桟由高地址到低地址，在基地址的基础上加上要分配的字节数就是桟的起点
    mov [es:esi+0x2c],eax;eax为基地址
    mov ebx,0xffffe                    ;段长度（界限）
    mov ecx,0x00c0b600                 ;4KB粒度，读写，特权级1
    call sys_routine_seg_sel:make_seg_descriptor
    mov ebx,esi
    call fill_descriptor_in_ldt
    or cx,0000_0000_0000_0001B
    mov [es:esi+0x30],cx
    mov dword [es:esi+0x32],0;初始esp指针
    
    ;创建2特权级堆栈
    mov ecx,4096
    mov eax,ecx                        ;为生成堆栈高端地址做准备
    mov [es:esi+0x36],ecx
    shr [es:esi+0x36],12               ;登记2特权级堆栈尺寸到TCB
    call sys_routine_seg_sel:allocate_memory
    add eax,ecx                        ;堆栈必须使用高端地址为基地址
    mov [es:esi+0x3a],ecx              ;登记2特权级堆栈基地址到TCB
    mov ebx,0xffffe                    ;段长度（界限）
    mov ecx,0x00c0d600                 ;4KB粒度，读写，特权级2
    call sys_routine_seg_sel:make_seg_descriptor
    mov ebx,esi                        ;TCB的基地址
    call fill_descriptor_in_ldt
    or cx,0000_0000_0000_0010          ;设置选择子的特权级为2
    mov [es:esi+0x3e],cx               ;登记2特权级堆栈选择子到TCB
    mov dword [es:esi+0x40],0          ;登记2特权级堆栈初始ESP到TCB
    
    ;在GDT中写入LDT描述符表的起始位置
    mov eax,[es:esi+0x0c];基地址
    mov ebx,word[es:esi+0x0a];段界限
    mov ecx,0x00408200
    call sys_routine_seg_sel:make_seg_descriptor;在使用时将当前LDT描述符加载入LDTR之中
    call sys_routine_seg_sel:set_up_gdt_descriptor
    mov [es:esi+0x10],cx               ;登记LDT选择子到TCB中
    ;创建tss段
    mov ecx,104
    mov [es:esi+0x12],cx
    dec word [es:esi+0x12]             ;登记TSS界限值到TCB 
    call sys_routine_seg_sel:allocate_memory
 ;登记基本的TSS表格内容
    mov word [es:ecx+0],0              ;反向链=0
      
    mov edx,[es:esi+0x24]              ;登记0特权级堆栈初始ESP
    mov [es:ecx+4],edx                 ;到TSS中
      
    mov dx,[es:esi+0x22]               ;登记0特权级堆栈段选择子
    mov [es:ecx+8],dx                  ;到TSS中
      
    mov edx,[es:esi+0x32]              ;登记1特权级堆栈初始ESP
    mov [es:ecx+12],edx                ;到TSS中

    mov dx,[es:esi+0x30]               ;登记1特权级堆栈段选择子
    mov [es:ecx+16],dx                 ;到TSS中

    mov edx,[es:esi+0x40]              ;登记2特权级堆栈初始ESP
    mov [es:ecx+20],edx                ;到TSS中

    mov dx,[es:esi+0x3e]               ;登记2特权级堆栈段选择子
    mov [es:ecx+24],dx                 ;到TSS中

    mov dx,[es:esi+0x10]               ;登记任务的LDT选择子
    mov [es:ecx+96],dx                 ;到TSS中
      
    mov dx,[es:esi+0x12]               ;登记任务的I/O位图偏移
    mov [es:ecx+102],dx                ;到TSS中 
      
    mov word [es:ecx+100],0            ;T=0
       
    ;在GDT中登记TSS描述符
    mov eax,[es:esi+0x14]              ;TSS的起始线性地址
    movzx ebx,word [es:esi+0x12]       ;段长度（界限）
    mov ecx,0x00408900                 ;TSS描述符，特权级0
    call sys_routine_seg_sel:make_seg_descriptor
    call sys_routine_seg_sel:set_up_gdt_descriptor
    mov [es:esi+0x18],cx               ;登记TSS选择子到TCB

    pop es                             ;恢复到调用此过程前的es段 
    pop ds                             ;恢复到调用此过程前的ds段
    popad
      
    ret 8                              ;丢弃调用本过程前压入的参数 
;-------------------------------------------------------------------------------
;创建tcb调用链,ecx中存储了
append_to_tcb_link:
    push eax
    push edx
    push es
    push ds

    mov eax,core_data_seg_sel
    mov ds,eax
    mov eax,mem_0_4_gb_seg_sel
    mov es,eax
    
    mov [es:ecx+0x00],0;将当前tcb块中的指针清零

    mov eax,[tcb_chain];将tcb头指针指向的地址放入内存
    or eax,eax;对比看头指针是否为空 
    jz .notcb;如果为空就跳转
  .searc:;若指针不为空
    mov edx,eax;将首块tcb地址放入edx中
    mov eax,[es:edx+0x00];从首块tcb块的指针中拿到下一块的地址
    or eax,eax;对比该指针是否为空
    jnz .searc;若不为空则循环向后找到最后一个tcb块

    mov [es:edx+0x00],ecx;将新的tcb块首地址放入最后一个块的指针内
    jmp .retcp
  .notcb:
    mov [tcb_chain],ecx
  .retcp
    pop es
    pop ds
    pop edx
    pop eax
;-------------------------------------------------------------------------------
start:
;初始化
    mov ecx,core_data_seg_sel
    mov ds,ecxmake_seg_descriptor
    mov ebx,massage_1
    call sys_routine_seg_sel:put_string
    ;显示cpu信息
    mov eax,0x80000002
    cpuid
    mov [cpu_brand + 0x00],eax
    mov [cpu_brand + 0x04],ebx
    mov [cpu_brand + 0x08],ecx
    mov [cpu_brand + 0x0c],edx

    mov eax,0x80000003
    cpuid
    mov [cpu_brand + 0x10],eax
    mov [cpu_brand + 0x14],ebx
    mov [cpu_brand + 0x18],ecx
    mov [cpu_brand + 0x1c],edx

    mov eax,0x80000004
    cpuid
    mov [cpu_brand + 0x20],eax
    mov [cpu_brand + 0x24],ebx
    mov [cpu_brand + 0x28],ecx
    mov [cpu_brand + 0x2c],edx

    mov ebx,cpu_bran0
    call sys_routine_seg_sel:put_string
    mov ebx,cpu_brand
    call sys_routine_seg_sel:put_string
    mov ebx,cpu_brnd1
    call sys_routine_seg_sel:put_string
    ;安装调用门
    mov edi,salt;起始地址
    mov ecx,salt_items;条目数量,循环条件

;创建调用门描述符
.b3:
    push ecx
    mov eax,[edi+256];第一条调用的入口点
    mov bx,[edi+260];段选择子
    mov cx,1_11_0_1100_000_00000B;特权及为3的调用门
    call sys_routine_seg_sel:make_seg_descriptor;拼凑门描述符
    call sys_routine_seg_sel:set_up_gdt_descriptor;安装门描述符
    mov [edi+260],cx;将原有的段选择子换成门描述符选择子
    add edi,salt_item_len;让edi指向下一条符号表的条目
    pop ecx
    loop .b3
;测试门描述符
    mov ebx,message_2
    call [salt_1+256];调用第一条符号表中的门描述符选择子
    mov ebx,message_3
    call sys_routine_seg_sel:put_string
;创建tcb链
    mov ecx,0x46
    call sys_routine_seg_sel:allocate_memory
    call append_to_tcb_link
;从磁盘读取用户程序
    push 50;压入磁盘的逻辑扇区号
    push ecx;将tcb分配的地址压入桟中
    call load_relocate_program;

    mov ebx,do_status
    call sys_routine_seg_sel:put_string

    


;
;===============================================================================

core_end:
