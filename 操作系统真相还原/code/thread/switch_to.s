[bits 32]
section .text
global switch_to
switch_to:
 push esi
 push edi 
 push ebx
 push ebp
 mov eax,[esp+20];从栈中取得当前线程的地址
 mov [eax],esp;将当前栈内的内容交给结构体储存

 mov eax,[esp+24];从栈中取得待切换线程的地址
 mov esp,[eax]
 pop ebp
 pop ebx
 pop edi 
 pop esi
 ret
