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
   mov ecx,[KERNEL_BIN_BASE_ADDR+44];表项数目
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
mov edi,[ebp+8]
mov esi,[ebp+12]
mov ecx,[ebp+16]
req movsb
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
;-------------------------------------------------------------------------------
							 ; eax=LBA扇区号
							 ; ebx=将数据写入的内存地址
							 ; ecx=读入的扇区数
      mov esi,eax	   ; 备份eax
      mov di,cx		   ; 备份扇区数到di
;读写硬盘:
;第1步：设置要读取的扇区数
      mov dx,0x1f2
      mov al,cl
      out dx,al            ;读取的扇区数

      mov eax,esi	   ;恢复ax

;第2步：将LBA地址存入0x1f3 ~ 0x1f6

      ;LBA地址7~0位写入端口0x1f3
      mov dx,0x1f3                       
      out dx,al                          

      ;LBA地址15~8位写入端口0x1f4
      mov cl,8
      shr eax,cl
      mov dx,0x1f4
      out dx,al

      ;LBA地址23~16位写入端口0x1f5
      shr eax,cl
      mov dx,0x1f5
      out dx,al

      shr eax,cl
      and al,0x0f	   ;lba第24~27位
      or al,0xe0	   ; 设置7～4位为1110,表示lba模式
      mov dx,0x1f6
      out dx,al

;第3步：向0x1f7端口写入读命令，0x20 
      mov dx,0x1f7
      mov al,0x20                        
      out dx,al

;;;;;;; 至此,硬盘控制器便从指定的lba地址(eax)处,读出连续的cx个扇区,下面检查硬盘状态,不忙就能把这cx个扇区的数据读出来

;第4步：检测硬盘状态
  .not_ready:		   ;测试0x1f7端口(status寄存器)的的BSY位
      ;同一端口,写时表示写入命令字,读时表示读入硬盘状态
      nop
      in al,dx
      and al,0x88	   ;第4位为1表示硬盘控制器已准备好数据传输,第7位为1表示硬盘忙
      cmp al,0x08
      jnz .not_ready	   ;若未准备好,继续等。

;第5步：从0x1f0端口读数据
      mov ax, di	   ;以下从硬盘端口读数据用insw指令更快捷,不过尽可能多的演示命令使用,
			   ;在此先用这种方法,在后面内容会用到insw和outsw等

      mov dx, 256	   ;di为要读取的扇区数,一个扇区有512字节,每次读入一个字,共需di*512/2次,所以di*256
      mul dx
      mov cx, ax	   
      mov dx, 0x1f0
  .go_on_read:
      in ax,dx		
      mov [ebx], ax
      add ebx, 2
			  ; 由于在实模式下偏移地址为16位,所以用bx只会访问到0~FFFFh的偏移。
			  ; loader的栈指针为0x900,bx为指向的数据输出缓冲区,且为16位，
			  ; 超过0xffff后,bx部分会从0开始,所以当要读取的扇区数过大,待写入的地址超过bx的范围时，
			  ; 从硬盘上读出的数据会把0x0000~0xffff的覆盖，
			  ; 造成栈被破坏,所以ret返回时,返回地址被破坏了,已经不是之前正确的地址,
			  ; 故程序出会错,不知道会跑到哪里去。
			  ; 所以改为ebx代替bx指向缓冲区,这样生成的机器码前面会有0x66和0x67来反转。
			  ; 0X66用于反转默认的操作数大小! 0X67用于反转默认的寻址方式.
			  ; cpu处于16位模式时,会理所当然的认为操作数和寻址都是16位,处于32位模式时,
			  ; 也会认为要执行的指令是32位.
			  ; 当我们在其中任意模式下用了另外模式的寻址方式或操作数大小(姑且认为16位模式用16位字节操作数，
			  ; 32位模式下用32字节的操作数)时,编译器会在指令前帮我们加上0x66或0x67，
			  ; 临时改变当前cpu模式到另外的模式下.
			  ; 假设当前运行在16位模式,遇到0X66时,操作数大小变为32位.
			  ; 假设当前运行在32位模式,遇到0X66时,操作数大小变为16位.
			  ; 假设当前运行在16位模式,遇到0X67时,寻址方式变为32位寻址
			  ; 假设当前运行在32位模式,遇到0X67时,寻址方式变为16位寻址.

      loop .go_on_read
      ret