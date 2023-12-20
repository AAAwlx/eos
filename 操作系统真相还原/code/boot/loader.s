%include "boot.inc"
   section lodar vstart=LOADER_BASE_ADDR
;构建gdt及其内部的描述符
   GDT_BASE:   dd    0x00000000 
	       dd    0x00000000

   CODE_DESC:  dd    0x0000FFFF 
	       dd    DESC_CODE_HIGH4

   DATA_STACK_DESC:  dd    0x0000FFFF
		     dd    DESC_DATA_HIGH4

   VIDEO_DESC: dd    0x80000007	       ; limit=(0xbffff-0xb8000)/4k=0x7
	       dd    DESC_VIDEO_HIGH4  ; 此时dpl为0

   GDT_SIZE   equ   $ - GDT_BASE
   GDT_LIMIT   equ   GDT_SIZE -	1 
   times 60 dq 0					 ; 此处预留60个描述符的空位(slot)
   SELECTOR_CODE equ (0x0001<<3) + TI_GDT + RPL0         ; 相当于(CODE_DESC - GDT_BASE)/8 + TI_GDT + RPL0
   SELECTOR_DATA equ (0x0002<<3) + TI_GDT + RPL0	 ; 同上
   SELECTOR_VIDEO equ (0x0003<<3) + TI_GDT + RPL0	 ; 同上 

   ; total_mem_bytes用于保存内存容量,以字节为单位,此位置比较好记。
   ; 当前偏移loader.bin文件头0x200字节,loader.bin的加载地址是0x900,
   ; 故total_mem_bytes内存中的地址是0xb00.将来在内核中咱们会引用此地址
   total_mem_bytes dd 0					 
   ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

   ;以下是定义gdt的指针，前2字节是gdt界限，后4字节是gdt起始地址
   gdt_ptr  dw  GDT_LIMIT 
	    dd  GDT_BASE

   ;人工对齐:total_mem_bytes4字节+gdt_ptr6字节+ards_buf244字节+ards_nr2,共256字节
   ards_buf times 244 db 0
   ards_nr dw 0		      ;用于记录ards结构体数量
;程序主体部分
loaderstart:
;获取内存大小
xor ebx, ebx		      ;第一次调用时，ebx值要为0
  mov edx, 0x534d4150	      ;edx只赋值一次，循环体中不会改变
  mov di, ards_buf	
.by_Oxe820:;使用0xe820方法
  mov eax,0x0000e820
  mov ecx,20
  int 0x15
  jc .try_to_0xe801
  add di,cx
  inc word [ards_nr]
  cmp ebx,0
  jnz .by_Oxe820
;选出最大的内存块
  mov cx,[ards_nr]
  mov ebx,ards_buf
xor edx,edx
.find_max:
mov eax,[ebx]
add eax,[ebx+8]
add ebx, 20
cmp edx,eax
jge .next_adr
mov edx,eax
.next_adr:
loop .find_max
jmp .mem_get_ok
.try_to_0xe801:
mov ax,0xe801
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
.mem_get_ok:
mov [total_mem_bytes],edx
in al,0x92
or al,0000_0010B
out 0x92,al
lgdt [gdt_ptr]
mov eax,cr0
or eax,0x00000001;开启pe位
mov cr0,eax
jmp dword SELECTOR_CODE:flush
.error_hlt:
  hlt
[bits 32]
flush:
mov ax,SELECTOR_DATA
mov ds,ax
mov ss,ax
mov es,ax
mov esp,LOADER_STACK_TOP
mov ax,SELECTOR_VIDEO
mov gs,ax
mov eax,KERNEL_START_SECTOR
mov ebx,KERNEL_BIN_BASE_ADDR
mov ecx,200
call rd_disk_m_32
;创建分页机制
call make_page
;给各段的位置加0xc0000000
sgdt [gdt_ptr]
mov ebx,[gdt_ptr+2]
or dword[ebx+0x18+4],0xc0000000
add dword[gdt_ptr+2],0xc0000000
add esp,0xc0000000
mov eax,PAGE_DIR_TABLE_POS
mov cr3,eax;
; 打开cr0的pg位(第31位)
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
   call init_kernel
   
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
   mov cx,[KERNEL_BIN_BASE_ADDR+44];表项数目
.each_segment:
  cmp byte [ebx+0],PT_NULL
  je .PTNULL

  push dword [ebx+16];表项的尺寸
  mov eax,[ebx+4];表项距离文件头的偏移量
  add eax,KERNEL_BIN_BASE_ADDR;
  push eax;表段的起始地址,源地址
  push dword[ebx+8];在编译时生成的程序被加载的目的地址
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
   mov edi, [ebp + 8]	   ; dst
   mov esi, [ebp + 12]	   ; src
   mov ecx, [ebp + 16]	   ; size
   rep movsb
   pop ecx
   pop ebp
   ret
;创建分页的代码
make_page:
mov ecx,4096
mov esi,0
.clean_page_dir:
mov byte[PAGE_DIR_TABLE_POS+esi],0
inc esi
loop .clean_page_dir
.create_page:
mov eax,PAGE_DIR_TABLE_POS
add eax,0x1000
mov ebx,eax

or eax,PG_US_U | PG_RW_W | PG_P
mov [PAGE_DIR_TABLE_POS+0x0],eax
mov [PAGE_DIR_TABLE_POS+0xc00],eax
sub eax,0x1000
mov [PAGE_DIR_TABLE_POS+4092],eax
;设置低1m内容
mov ecx,256
mov esi,0
mov edx,PG_US_U | PG_RW_W | PG_P

.create_pte:
mov [ebx+esi *4],edx
add edx,4096;增加一页的大小
inc esi
loop .create_pte
;将第769位之后的1GB系统空间
mov eax,PAGE_DIR_TABLE_POS
add eax,0x2000
or eax,PG_US_U | PG_RW_W | PG_P
mov ebx, PAGE_DIR_TABLE_POS
mov esi,769
mov ecx,254
.create_pde:
mov [ebx+esi*4],eax;
inc esi
add eax,0x1000
loop .create_pde
ret
rd_disk_m_32:	   
mov esi,eax ;备份eax，因为后面会用到ax寄存器
  mov di,cx   ;备份cx到di
  ;读写磁盘
  ;第一步：设置读取的扇区数
  mov dx,0x1f2 ;存放端口到0x1f2对应Sector count 寄存器用来指定待读取或待写入的扇区数
  mov al,cl    ;将读取的扇区数存放如al
  out dx,al    ;向端口写数据
  mov eax,esi  ;恢复ax

  ;第二步写入地址
  ;LBA 地址 7~0 位写入端口 0x1f3
  mov dx,0x1f3
  out dx,al

  ;LBA 地址 15~8 位写入端口 0x1f4
  mov cl,8
  shr eax,cl ;右移动8位将15~8位移到al
  mov dx,0x1f4
  out dx,al

  ;LBA 地址 23~16 位写入端口 0x1f5
  shr eax,cl ;再右移动8位
  mov dx,0x1f5
  out dx,al

  shr eax,cl    ;在右移动8位
  and al,0x0f   ;与1111(只保留低四位)
  or al,0xe0    ;或11100000 lba 第 24~27 位（在高4位或上1110,表示 lba(扇区从0开始编号) 模式,主盘）
  mov dx,0x1f6  
  out dx,al

  ;第三步写入命令
  mov dx ,0x1f7
  mov al ,0x20
  out dx ,al

  ;第四位：检测硬盘状态
.not_ready:
  nop ;空指令，用于debug
  in al,dx  ;端口也是0x1F7
  and al,0x88 ;与1000 1000 ，只保留第三位和第七位
  ;第三位为1表示银盘准备好传输数据,第七位表示硬盘忙
  cmp al ,0x08 ;进行比较判断硬盘是否准备好，如果相等表示(硬盘忙)没有准备好
  jnz .not_ready ;没有准备好，继续等

  ;第五步：从0x1f0读取数据
 
  ;di 为要读取的扇区数,一个扇区有 512 字节,每次读入一个字共需 di*512/2 次,所以 di*256
  ;下面四行计算读取次数，并存放在ax当中，
  mov ax,di   ;此时di寄存器存放的是的读取扇区个数
  mov dx,256  
  mul dx ;dx*ax存放在ax中
  mov cx,ax

  mov dx,0x1f0
.go_on_read:
  in ax,dx ;进行读取
  mov [ebx],ax
  add ebx,2
  loop .go_on_read
  ret
