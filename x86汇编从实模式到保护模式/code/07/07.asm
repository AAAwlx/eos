jmp near start
message db '1+2+3+...+100='
start:
;初始化寄存器
    mov ax,0x7c0
    mov ds,ax
    mov ax,0xb800
    mov es,ax
;显示字符串
    mov si,message;ds数据段作为si的基址
    mov di,0
    mov cx,start-message
    @g:
        mov ax,[si]
        mov [es:di],ax
        inc di;
        mov byte [es:di],0x07;设置显示颜色
        inc si
        inc di
        loop @g
;计算1～100的和
    xor ax ,ax;将ax中的值清0
    mov cx,1
    @f:    
        add ax,cx
        inc cx
        cmp cx,100
        jle @f
;将每个数位拆分出来
    xor cx,cx
    mov ss,cx
    mov sp,cx
    mov bx,10
    xor cx,cx
    @d:
        inc cx
        xor dx,dx
        div bx
        or dl,0x30
        push dx
        cmp ax,0
        jne @d
;显示字符
    @a:
        pop dx
        mov [es:di],dl
        inc di
        mov [es:di],0x07
        inc di
        loop @a
    jmp near $;跳转到自身指令结尾。相当与nop指令
times 510-($-$$) db 0
                 db 0x55,0xaa
