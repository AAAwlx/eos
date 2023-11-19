;数据段
;定义gdt描述符各段
%include "boot.inc"
SECTION loader vstart=LOADER_BASE_ADDR
LOADER_TOP equ LOADER_BASE_ADDR
GDT_BASE dd    0x00000000 
	     dd    0x00000000
CODE_DECS dd 0x0000ffff
          dd DESC_CODE_HIGH4
DATA_STACK_DECS dd 0x0000ffff
                dd DESC_DATA_HIGH4
VIDEO_DECS dd 0x0000ffff
            dd DESC_VIDEO_HIGH4
SECTION_COED equ (0X0001<<3)+TI_GDT+RPL0;序号前移3位，将段选择子类型和权限加上
SECTION_DATA equ (0X0002<<3)+TI_GDT+RPL0
SECTION_VIDEO equ (0X0003<<3)+TI_GDT+RPL0

GDT_SIZE equ $-GDT_BASE
GDT_LIMIT equ GDT_SIZE-1
times 60 dq 0;为gdt预留空间
;定义存储内存大小的字段
total_mem_bytes dd 0;四字节总的内存大小
gdt_ptr dw GDT_LIMIT;gdt指针前两位是双字
        dd GDT_BASE;gdt界限
ards_buf times 244 db  0
ards_nr dw 0;一共有多少个结构体
;程序主体部分
loaderstart:
;获取内存大小
mov edx,0x534d4150
xor ebx,ebx
mov di,ards_buf
.by_Oxe820:;使用0xe820方法
mov eax,0xe820
mov ecx,20
int 0x15
jc .try_to_0xe801
inc word [ards_nr]
add di,20
cmp ebx,0
jnz by_Oxe820
;选出最大的内存块
mov cx,ards_nr
mov ebx,ards_buf
xor edx,edx
.find_max:
mov eax,[ebx]
add eax,[ebx+8]
cmp edx,eax
jge .next_adr
mov edx,eax
.next_adr:
add ebx,20
loop .find_max
jmp .mem_get_ok
try_to_0xe801:
mov eax,0xe801
int 0x15
jc .try_to_0x88
mov cx,0x400
mul cx
shl edx,16
and eax,0x0000ffff;将高十六位清零
or edx,eax
add edx,0x100000
mov esi,edx
xor eax,eax
mov ax,bx
mov ecx,0x100000
mul ecx
add eax,esi
mov edx,eax
jmp .mem_get_ok 
.try_to_0x88:
mov ah,0x88
int 0x15
jc .error_hlt
and eax,0x0000ffff
mov cx,0x400
mul cx
shl edx,16
or edx,eax
add edx,0x100000
add edx,
.mem_get_ok:
mov [total_mem_bytes],edx
in eax,0x92
or eax,0000_0010B
out 0x92,eax
lgdt [gdt_ptr]
mov eax,cr0
or eax,0x0008
mov cr0,eax
jmp dword SECTION_COED:p_mode_start
.error_hlt:
  hlt
[bits 32]
flush:
mov ax,SECTION_DATA
mov ds,ax
mov ss,ax
mov es,ax
mov esp,LOADER_STACK_TOP
mov ax,SECTION_VIDEO
mov gs,ax
mov eax,KERNEL_IMAGE_BASE_ADDR
mov ebx,KERNEL_BIN_BASE_ADDR
mov ecx,200
call read_disk
;创建分页机制
call make_page
;给各段的位置加0xc0000000
sgdt [gdt_ptr]
mov ebx,[gdt_ptr+2]
or dword[ebx+0x18+4],0xc0000000
or [gdt_ptr+2],0xc0000000
add esp,0xc0000000
mov eax,PAGE_DIR_TABLE_POS
mov cr3,eax;
mov eax,cr0
or eax,0x80000000
mov cr0,eax
lgdt [gdt_ptr];
;显示信息

   mov byte [gs:160], 'V'     ;视频段段基址已经被更新,用字符v表示virtual addr
   mov byte [gs:162], 'i'     ;视频段段基址已经被更新,用字符v表示virtual addr
   mov byte [gs:164], 'r'     ;视频段段基址已经被更新,用字符v表示virtual addr
   mov byte [gs:166], 't'     ;视频段段基址已经被更新,用字符v表示virtual addr
   mov byte [gs:168], 'u'     ;视频段段基址已经被更新,用字符v表示virtual addr
   mov byte [gs:170], 'a'     ;视频段段基址已经被更新,用字符v表示virtual addr
   mov byte [gs:172], 'l'     ;视频段段基址已经被更新,用字符v表示virtual addr
   
;;;;;;;;;;;;;;;;;;;;;;;;;;;;  此时不刷新流水线也没问题  ;;;;;;;;;;;;;;;;;;;;;;;;
;由于一直处在32位下,原则上不需要强制刷新,经过实际测试没有以下这两句也没问题.
;但以防万一，还是加上啦，免得将来出来莫句奇妙的问题.
   jmp SELECTOR_CODE:enter_kernel	  ;强制刷新流水线,更新gdt
enter_kernel:    
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
   mov byte [gs:320], 'k'     ;视频段段基址已经被更新
   mov byte [gs:322], 'e'     ;视频段段基址已经被更新
   mov byte [gs:324], 'r'     ;视频段段基址已经被更新
   mov byte [gs:326], 'n'     ;视频段段基址已经被更新
   mov byte [gs:328], 'e'     ;视频段段基址已经被更新
   mov byte [gs:330], 'l'     ;视频段段基址已经被更新

   mov byte [gs:480], 'w'     ;视频段段基址已经被更新
   mov byte [gs:482], 'h'     ;视频段段基址已经被更新
   mov byte [gs:484], 'i'     ;视频段段基址已经被更新
   mov byte [gs:486], 'l'     ;视频段段基址已经被更新
   mov byte [gs:488], 'e'     ;视频段段基址已经被更新
   mov byte [gs:490], '('     ;视频段段基址已经被更新
   mov byte [gs:492], '1'     ;视频段段基址已经被更新
   mov byte [gs:494], ')'     ;视频段段基址已经被更新
   mov byte [gs:496], ';'     ;视频段段基址已经被更新
   call kernel_init
   mov esp, 0xc009f000
   jmp KERNEL_ENTRY_POINT                 ; 用地址0x1500访问测试，结果ok

;初始化内核
init_kernel:
   xor eax, eax
   xor ebx, ebx		;ebx记录程序头表地址
   xor ecx, ecx		;cx记录程序头表中的program header数量
   xor edx, edx		;dx 记录program header尺寸,即e_phentsize
   mov dx,[KERNEL_BIN_BASE_ADDR+42];一个program的大小
   mov ebx,[KERNEL_BIN_BASE_ADDR+28]; 偏移文件开始部分28字节的地方是e_phoff,表示第1 个program header在文件中的偏移量
   add ebx,KERNEL_BIN_BASE_ADDR
   mov ecx,[KERNEL_BIN_BASE_ADDR+44];表项数目
.each_segment:
  cmp byte[ebx],PTNULL
  je .PTNULL
  push dword[ebx+16];表项的尺寸
  mov eax,[ebx+4];表项距离文件头的偏移量
  add eax,KERNEL_BIN_BASE_ADDR;
  push eax;表段的起始地址,源地址
  push,[ebx+8];在编译时生成的程序被加载的目的地址
  call mem_cpy
  add esp,12;将之前传入的参数丢弃
.PTNULL:
  add ebx,edx
  loop .each_segment
  ret
mem_cpy:
cld 
push ebp
mov ebp,esp
push ecx
mov edi,[ebp+8]
mov esi,[ebp+12]
mov ecx,[ebp+16]
req movsb
pop ecx
pop ebp
ret
;创建分页的代码
make_page:
mov ecx,0x4096
mov esi,0
.clean_page_dir:
mov [PAGE_DIR_TABLE_POS+si],0
inc esi
loop .clean_page_dir
.create_page
mov eax,PAGE_DIR_TABLE_POS
add eax,0x1000
mov eax,ebx

or eax,PG_RW_W|PG_US_S|PG_P
mov [PAGE_DIR_TABLE_POS],eax
mov [PAGE_DIR_TABLE_POS+0xc00],eax
sub eax,0x1000
mov [PAGE_DIR_TABLE_POS+4092],eax
;设置低1m内容
mov cx,256
mov edx,PG_RW_W|PG_US_S|PG_P
mov esi,0
.create_pte:
mov [PAGE_DIR_TABLE_POS+si *4],edx
add edx,4096;增加一页的大小
inc esi
loop .create_pte
;将第769位之后的1GB系统空间
mov ebx,PAGE_DIR_TABLE_POS
add eax,0x2000
mov si,769
mov ecx,254
.create_pde:
or eax,PG_RW_W|PG_US_U|PG_P
mov [PAGE_DIR_TABLE_POS+si*4],eax;
inc si
add ax,0x1000
loop .create_pde
ret
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