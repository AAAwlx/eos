#include "keyboard.h"
#include "global.h"
#include "interrupt.h"
#include "io.h"
#include "print.h"
#include "sysnc.h"
#include"ioqueue.h"
struct ioqueue kbd_buf;
#define KBD_BUF_PORT 0x60  // 键盘的端口
#define char_invisible 0
#define ctrl_l_char char_invisible
#define ctrl_r_char char_invisible
#define shift_l_char char_invisible
#define shift_r_char char_invisible
#define alt_l_char char_invisible
#define alt_r_char char_invisible
#define caps_lock_char char_invisible
#define shift_l_make 0x2a
#define shift_r_make 0x36
#define alt_l_make 0x38
#define alt_r_make 0xe038
#define alt_r_break 0xe0b8
#define ctrl_l_make 0x1d
#define ctrl_r_make 0xe01d
#define ctrl_r_break 0xe09d
#define caps_lock_make 0x3a
#define esc '\033'  // 八进制表示字符,也可以用十六进制'\x1b'
#define backspace '\b'
#define tab '\t'
#define enter '\r'
#define delete '\177'  // 八进制表示字符,十六进制为'\x7f'static
// 控制键状态
static bool ctrl_status /*ctrl键状态*/, shift_status /*shift键状态*/,
    alt_status /*alt键状态*/, caps_lock_status /*capslock键状态*/,
    ext_scancode /*是否有连续的多位通断码*/;
char keymap[][2] = {
    /* 扫描码   未与shift组合  与shift组合*/
    /* ---------------------------------- */
    /* 0x00 */ {0, 0},
    /* 0x01 */ {esc, esc},
    /* 0x02 */ {'1', '!'},
    /* 0x03 */ {'2', '@'},
    /* 0x04 */ {'3', '#'},
    /* 0x05 */ {'4', '$'},
    /* 0x06 */ {'5', '%'},
    /* 0x07 */ {'6', '^'},
    /* 0x08 */ {'7', '&'},
    /* 0x09 */ {'8', '*'},
    /* 0x0A */ {'9', '('},
    /* 0x0B */ {'0', ')'},
    /* 0x0C */ {'-', '_'},
    /* 0x0D */ {'=', '+'},
    /* 0x0E */ {backspace, backspace},
    /* 0x0F */ {tab, tab},
    /* 0x10 */ {'q', 'Q'},
    /* 0x11 */ {'w', 'W'},
    /* 0x12 */ {'e', 'E'},
    /* 0x13 */ {'r', 'R'},
    /* 0x14 */ {'t', 'T'},
    /* 0x15 */ {'y', 'Y'},
    /* 0x16 */ {'u', 'U'},
    /* 0x17 */ {'i', 'I'},
    /* 0x18 */ {'o', 'O'},
    /* 0x19 */ {'p', 'P'},
    /* 0x1A */ {'[', '{'},
    /* 0x1B */ {']', '}'},
    /* 0x1C */ {enter, enter},
    /* 0x1D */ {ctrl_l_char, ctrl_l_char},
    /* 0x1E */ {'a', 'A'},
    /* 0x1F */ {'s', 'S'},
    /* 0x20 */ {'d', 'D'},
    /* 0x21 */ {'f', 'F'},
    /* 0x22 */ {'g', 'G'},
    /* 0x23 */ {'h', 'H'},
    /* 0x24 */ {'j', 'J'},
    /* 0x25 */ {'k', 'K'},
    /* 0x26 */ {'l', 'L'},
    /* 0x27 */ {';', ':'},
    /* 0x28 */ {'\'', '"'},
    /* 0x29 */ {'`', '~'},
    /* 0x2A */ {shift_l_char, shift_l_char},
    /* 0x2B */ {'\\', '|'},
    /* 0x2C */ {'z', 'Z'},
    /* 0x2D */ {'x', 'X'},
    /* 0x2E */ {'c', 'C'},
    /* 0x2F */ {'v', 'V'},
    /* 0x30 */ {'b', 'B'},
    /* 0x31 */ {'n', 'N'},
    /* 0x32 */ {'m', 'M'},
    /* 0x33 */ {',', '<'},
    /* 0x34 */ {'.', '>'},
    /* 0x35 */ {'/', '?'},
    /* 0x36	*/ {shift_r_char, shift_r_char},
    /* 0x37 */ {'*', '*'},
    /* 0x38 */ {alt_l_char, alt_l_char},
    /* 0x39 */ {' ', ' '},
    /* 0x3A */ {caps_lock_char, caps_lock_char}
    /*其它按键暂不处理*/
};
struct ioqueue* buf;
static void intr_keyboard_handler(void) {
    bool ctrl_down_last = ctrl_status;  // 记录上次是否有键盘状态的变化
    bool shift_down_last = shift_status;
    bool caps_lock_last = caps_lock_status;

    uint16_t code = inb(KBD_BUF_PORT);
    if (code == 0xe0) {
        ext_scancode = true;
        return;
    }
    if (ext_scancode) {
        code = ((0xe000) | code);  // 如果上一次是0xe0开头，将两次的码合并
        ext_scancode = false;  // 关闭e0标记
    }
    bool stat_code = ((code & 0x0080) != 0);  // 通断码的状态
    if (stat_code)                            // 如果是断码
    {
        uint16_t make_code = (code &= 0xff7f);
        if (make_code == ctrl_r_make || make_code == ctrl_l_make) {
            ctrl_status = false;
        } else if (make_code == alt_l_make || make_code == alt_r_make) {
            alt_status = false;
        } else if (make_code == shift_r_make || make_code == shift_l_make) {
            shift_status = false;
        }
        return;
    } else if ((code > 0x00 && code < 0x3b) || (code == alt_r_make) ||
               (code == ctrl_r_make))  // 如果是通码
    {
        bool shift = false;
        // 如果是符号数字键
        if ((code < 0x0e) || (code == 0x29) || \ 
	    (code == 0x1a) ||
            (code == 0x1b) || (code == 0x2b) || (code == 0x27) ||
            (code == 0x28) || (code == 0x33) || (code == 0x34) ||
            (code == 0x35)) {
            if (shift_down_last)  // 如果和shift结合
            {
                shift = true;
            }
        } else {
            // 如果是普通符号键
            if (shift_down_last && ctrl_down_last)  // 两键抵消
            {
                shift = false;
            } else if (shift_down_last || caps_lock_last)  // 两个键任意都能改变普通键的大小写
            {
                shift = true;
            } else {
                shift = false;
            }
        }
        uint8_t index = (code &= 0x00ff);
        char cur_char = keymap[index][shift];
        if (cur_char)  // 如果不是0
        {
            
             /*put_char(cur_char);
            return;*/
        
            if ((ctrl_down_last && cur_char == 'l')||(ctrl_down_last && cur_char == 'u')) {
	            cur_char -= 'a';
	        }
            if (!ioq_full(&kbd_buf))
            {
                ioq_putchar(&kbd_buf, cur_char);
            }
            return;
        } else {  // 如果是0,则是控制键盘
            if (code == ctrl_l_make || code == ctrl_r_make) {
                ctrl_status = true;
            } else if (code == shift_l_make || code == shift_r_make) {
                shift_status = true;
            } else if (code == alt_l_make || code == alt_r_make) {
                alt_status = true;
            } else if (code == caps_lock_make) {
                caps_lock_status = !caps_lock_status;
            }
        }

    } else  // 如果不是主键区
    {
       // put_str("unknown key\n");
    }
}
static void intr_keyboard_handler1(void) {
   put_char('k');
/* 必须要读取输出缓冲区寄存器,否则8042不再继续响应键盘中断 */
   inb(KBD_BUF_PORT);
   return;
}

void keyboard_init(void) {
    put_str("keyboard init start\n");
    ioqueue_init(&kbd_buf);
    register_hsndler(0x21, intr_keyboard_handler);
    put_str("keyboard init down\n");
}