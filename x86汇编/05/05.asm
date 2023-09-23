mov ax ,0xb800;显示缓冲区
mov es ,ax;使附加段寄存器指向显示缓冲区
;显示字符
mov byte [es:0x00],'H'
mov byte [es:0x01],0x07;指定显示的颜色
mov byte [es:0x02],'E'
mov byte [es:0x03],0x07
mov byte [es:0x04],'L'
mov byte [es:0x05],0x07
mov byte [es:0x06],'L'
mov byte [es:0x07],0x07
mov byte [es:0x08],'O'
mov byte [es:0x09],0x07
;将number地址的 各个位数拆解出来
mov ax ,number;获得标号地址
mov bx ,10
mov cx ,cs;获得代码段的地址
mov ds ,cx;将数据段地址的寄存器也指向代码段，将数据段和代码指令段混合到一起
mov dx ,0
div bx
mov [0x7c00+number+0x00],dl
xor dx ,dx;将dx中的值清零
div bx
mov [0x7c00+number+0x01],dl
xor dx ,dx
div bx
mov [0x7c00+number+0x02],dl
xor dx ,dx
div bx
mov [0x7c00+number+0x03],dl
xor dx ,dx
div bx
mov [0x7c00+number+0x04],dl
;用十进制显示偏移地址
mov al,[0x7c00+number+0x04]
add al,0x30
mov [es:0x0a],al
mov byte [es:0xb],0x04

mov al,[0x7c00+number+0x03]
add al,0x30
mov [es:0x0c],al
mov byte [es:0xd],0x04

mov al,[0x7c00+number+0x02]
add al,0x30
mov [es:0x0e],al
mov byte [es:0x0f],0x04

mov al,[0x7c00+number+0x01]
add al,0x30
mov [es:0x10],al
mov byte [es:0x11],0x04

mov al,[0x7c00+number+0x00]
add al,0x30
mov [es:0x12],al
mov byte [es:0x13],0x04
number db 0 ,0 ,0 ,0 ,0
 times 510-($-$$) db 0
                  db 0x55,0xaa;将开头的512字节填满，并将结尾设置为aa55