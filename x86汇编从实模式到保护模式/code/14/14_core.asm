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
;装载用户程序
load_relocate_program: 
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
    
    mov [es:ecx+0x00],0;将当前tcb指针清零

    mov eax,[tcb_chain]
    or eax,eax 
    jz .notcb
  .searc:
    mov edx,eax
    mov eax,[es:edx+0x00]
    or eax,eax
    jnz .searc

    mov [es:edx+0x00],ecx
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
    mov ds,ecx
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
;分配内存空间

;创建用户程序中对应的段描述符
;===============================================================================

core_end:
