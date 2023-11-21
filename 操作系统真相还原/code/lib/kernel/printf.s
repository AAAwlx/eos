TI_GDT equ 0
RPL0 equ 0
SELECTOR_VIDEO equ (0x0003<<3)+TI_GDT+PRL0

;定义一个数据缓冲区
SECTION .data
  str_buf dq 0
[bits 32]

SECTION .code
;打印字符串
global put_str
put_str:
	push ebx
	push ecx
	xor ecx,ecx
	mov ebx,[esp+12]
.goon:
	mov cl,[ebx]
	cmp cl,0
	jz .str_over
	push ecx
	call put_char
	add esp,4;将之前压入栈中的值舍弃
	inc ebx
	jmp .goon
.str_over:
	pop ecx
	pop ebx
	ret 
;打印单个字符
global put_char
put_char:
  pushad;备份当前所有寄存器的值
	mov ax,SELECTOR_VIDEO
	mov gs,ax;将显示选择子放入附加段选择器中
;光标位置分为高八位和低八位，先获取高八位
  mov dx,0x3d4
	mov al,0x0e
	out dx,al
	mov dx,0x3d5
	in al,dx
	mov ah,al;
	;再获取低八位
	mov dx,0x3d4
	mov al,0x0f
	out dx,al
	mov dx,0x3d5
	in al,dx
	mov bx,ax
	;从栈中获取待打印的字符
	mov ecx,[esp+36]
	;判断是否是普通字符
	cmp cl,0xd;回车
	jz .is_carring_return
	cmp cl,0xa;换行
	jz .is_line_feed
	cmp cl,0x8;回退
	jz .is_backspace
	jmp .put_other:;如果都不是是普通字符
	.is_backspace:
	dec bx
	shl,1
	mov byte[gs:bx],0x20
	inc bx
	mov byte[gs:bx],0x07
	shr bx,1
	jmp .set_cursor
	.put_char:
		shl bx,1
		mov [gs:bx],cx
		inc bx
		mov byte[gs:bx],0x07;显示属性
		shr bx,1
		inc bx
		jmp .end
	.is_carring_return:
	.is_line_feed:
		xor dx,dx
		mov ax,bx
		mov si,80
		div si
		sub bx,dx;将光标数除80得到所
		add bx,80
		.end:
			cmp bx,2000
			jl .set_cursor;若未超出屏幕大小则跳转至重设光标处若是没有则滚屏。
	roll_screen:;
	cld
	mov ecx,960
	mov si 0x8000
	mov di 0x80a0
	rep movsd 
	mov ebx 3480
	mov ecx 80
	.cls:
		mov word[gs:ebx],0x720
		add ebx,2
		loop .cls
	mov bx,1920;重置光标为滚屏后的光标
	.set_cursor:
	mov dx,0x3d4
	mov al,0x0e
	out dx,al
	mov dx,0x3d5
	mov al,bh
	out dx,al

	mov dx,0x3d4
	mov al,0x0f
	out dx,al
	mov dx,0x3d5
	mov al,bl
	mov dx,al
	popad
	ret
global put_int
put_int:
pushad
mov eax,[esp+4*9]
mov edx,eax;将edx备份
mov edi,7
mov ecx,8
mov ebx,str_buf
.16_4bits:
	and edx,0x00000000f;每次只处理四位，32位int型分八次处理
	cmp edx,9
	jg .is_A2F
	add edx,'0'
	jmp .store 
.is_A2F:
sub edx,10
add edx,'A'
.store:
mov [ebx+edi],dl 
dec edi 
shr eax,4
mov edx,eax
loop .16_4bits
ready_to_print:
inc edi;将已经减为-1（0xffffffff）的数据加为0
;以下这段将前方的0去掉
.cmp_prefix_0:
cmp edi,8
je .full0
go_on_skip:
mov cl,[str_buf+edi];将当前位移入cl中等待比较
inc edi 
cmp cl,'0';如果当前位是0就跳过
je .cmp_prefix_0;判断是不是八位数字都是0
dec edi;如果当前位不是0就将偏移量减一恢复当前位
jmp .put_num
.full0
mov cl ,'0'
.put_num:
push ecx
call put_char
add esp,4
inc edi
mov ecx,[str_buf+edi]
cmp edi,8
jl .put_num
popad
ret
