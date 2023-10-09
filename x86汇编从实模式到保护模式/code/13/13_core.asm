;常数定义，定义段选择子
core_code_seg_sel equ 0x38;7号段描述符，核心代码文本段
core_data_seg_sel equ 0x30;6号，核心数据段
sys_routine_seg_sel equ 0x28;5号，公共例程段
video_ram_seg_sel equ 0x20;4号，显示附加段
mem_0_4_gb_seg_sel equ 0x08;1号全局访问
;程序头部
core_length dd core_end
sys_routine_seg dd section.sys_routine_seg.start
core_data_seg dd section.core_data_seg.start
core_code_seg dd section.core_code.start
core_entry dd start
           dw core_code_seg_sel
;===============================================================================
SECTION core_data vstart = 0;内核数据段
;gdt的地址和界限
    pgat dw 0
         dd 0
;缓冲区
    ram_alloc dd 0x00100000
    
;字符信息
;===============================================================================
SECTION core_code vstart = 0;内核代码段
;显示cpu信息
start:
;从磁盘读取用户程序
;分配内存空间
;创建用户程序中对应的段描述符
;===============================================================================

SECTION sys_routine vstart = 0;共用例程api
;
;===============================================================================

core_end:;结尾标号
