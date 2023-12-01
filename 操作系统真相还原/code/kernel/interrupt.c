#include "io.h"
#include "interrupt.h"
#include "global.h"
#include"printf.h"
#define PIC_M_CTRL 0x20	       // 这里用的可编程中断控制器是8259A,主片的控制端口是0x20
#define PIC_M_DATA 0x21	       // 主片的数据端口是0x21
#define PIC_S_CTRL 0xa0	       // 从片的控制端口是0xa0
#define PIC_S_DATA 0xa1	       // 从片的数据端口是0xa1
#define IDT_DESC_CNT 0x21      // 目前总共支持的中断数

#define FLAGS_IF 0X00000200//将if位改为1
#define GET_FLAGS(EFLAG_VAR) asm volatile("pushfl;pop %0" : "=g"(EFLAG_VAR));//将
typedef struct {
    uint16_t lower_offset;//段内偏移低十六位
    uint16_t segment_selector;//对应代码段的选择子
    uint8_t dcount;//没用的八位
    uint8_t type;//位的类型
    uint16_t higt_offset;//段内偏移高十六位
} gate_descriptor;
intr_handler idt_table[IDT_DESC_CNT];//中断对应的处理程序
gate_descriptor idt[IDT_DESC_CNT];//中断描述符表
extern intr_handler idt_table_entry[IDT_DESC_CNT];//在外部定义了一个中断处理程序的入口点，kernel中的汇编程序地址
char* intr_name[IDT_DESC_CNT];//中断对应的报错信息
static void make_gate_descriptor( gate_descriptor* p_gdesc
,intr_handler function,uint8_t type,uint16_t segment_selector){
    p_gdesc->lower_offset = (uint32_t)function & 0x0000ffff;
    p_gdesc->segment_selector = segment_selector;
    p_gdesc->dcount = 0;
    p_gdesc->type = type;
    p_gdesc->higt_offset=((uint32_t)function) & 0xffff0000>>16;
}
static void idt_desc_init(){
    for (int i = 0; i < IDT_DESC_CNT;i++){
        make_gate_descriptor(&idt[i],  idt_table_entry[i], IDT_DESC_ATTR_DPL0,SELECTOR_K_CODE);
    }
    put_str("idt_desc_init done\n");
}
static void pic_init()
{
    outb(PIC_M_CTRL, 0x11);   // ICW1: 边沿触发,级联8259, 需要ICW4.
    outb (PIC_M_DATA, 0x20);   // ICW2: 起始中断向量号为0x20,也就是IR[0-7] 为 0x20 ~ 0x27.
    outb (PIC_M_DATA, 0x04);   // ICW3: IR2接从片. 
    outb (PIC_M_DATA, 0x01);   // ICW4: 8086模式, 正常EOI

    /* 初始化从片 */
    outb (PIC_S_CTRL, 0x11);    // ICW1: 边沿触发,级联8259, 需要ICW4.
    outb (PIC_S_DATA, 0x28);    // ICW2: 起始中断向量号为0x28,也就是IR[8-15] 为 0x28 ~ 0x2F.
    outb (PIC_S_DATA, 0x02);    // ICW3: 设置从片连接到主片的IR2引脚
    outb (PIC_S_DATA, 0x01);    // ICW4: 8086模式, 正常EOI

    /* 打开主片上IR0,也就是目前只接受时钟产生的中断 */
    outb (PIC_M_DATA, 0xfe);
    outb (PIC_S_DATA, 0xff);

    put_str("pic_init done\n");
}
static void general_intr_handler(uint8_t vec_nr)//中断处理函数的参数.
{
    if(vec_nr==0x27||vec_nr==0x2f){
        return;
    }
    put_str("int vector: 0x");
    put_int(vec_nr);
    put_char('\n');
}
static void exception_init()
{
    for (int i = 0; i < IDT_DESC_CNT;i++){
        idt_table[i] = general_intr_handler;//默认直接打印，以后会根据需求注册中断处理函数
        intr_name[i] = "unknown";
    }
    intr_name[0] = "#DE Divide Error";
    intr_name[1] = "#DB Debug Exception";
    intr_name[2] = "NMI Interrupt";
    intr_name[3] = "#BP Breakpoint Exception";
    intr_name[4] = "#OF Overflow Exception";
    intr_name[5] = "#BR BOUND Range Exceeded Exception";
    intr_name[6] = "#UD Invalid Opcode Exception";
    intr_name[7] = "#NM Device Not Available Exception";
    intr_name[8] = "#DF Double Fault Exception";
    intr_name[9] = "Coprocessor Segment Overrun";
    intr_name[10] = "#TS Invalid TSS Exception";
    intr_name[11] = "#NP Segment Not Present";
    intr_name[12] = "#SS Stack Fault Exception";
    intr_name[13] = "#GP General Protection Exception";
    intr_name[14] = "#PF Page-Fault Exception";
    // intr_name[15] 第15项是intel保留项，未使用
    intr_name[16] = "#MF x87 FPU Floating-Point Error";
    intr_name[17] = "#AC Alignment Check Exception";
    intr_name[18] = "#MC Machine-Check Exception";
    intr_name[19] = "#XF SIMD Floating-Point Exception";

}
//开中断并返回开中断前的状态 
enum intr_status intr_enable()
{
    enum intr_status old_status;
    if(get_status()==INTR_OFF){
        old_status = INTR_OFF;
        asm volatile("sti");
    } else {
        old_status = INTR_NO;
    }
    return old_status;
}
//关中断
enum intr_status intr_disable()
{
    enum intr_status old_status;
    if(get_status()==INTR_OFF){
        old_status = INTR_OFF; 
        
    } else {
        old_status = INTR_NO;
        asm volatile("cli" : : : "memory");
    }
    return old_status;
}
//设置中断状态
enum intr_status set_status(enum intr_status new_status)
{
    return (new_status & FLAGS_IF) ? intr_enable() : intr_disable();
}
//获取中断状态
enum intr_status get_status()
{
    uint32_t efalg=0;
    GET_FLAGS(efalg);
    return (efalg & FLAGS_IF) ? INTR_NO : INTR_OFF;
}
void idt_init() {
    put_str("idt_init start\n");
    idt_desc_init();	   // 初始化中断描述符表
    exception_init();	   // 异常名初始化并注册通常的中断处理函数
    pic_init();		   // 初始化8259A
    uint64_t idt_operand = ((sizeof(idt) - 1) | ((uint64_t)(uint32_t)idt << 16));
    asm volatile("lidt %0" : : "m" (idt_operand));
    put_str("idt_init done\n");
}