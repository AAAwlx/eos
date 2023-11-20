#include<stdio.h>

int main() {
    int ret_cnt = 0, test = 0;
    char *fmt = "hello, world\n";

    asm volatile(
        "pushl %1\n\t"
        "call printf\n\t"
        "addl $4,%%esp\n\t"
        "movl $6, %0"
        : "=a" (ret_cnt)
        : "m" (fmt),"r"(test)
    );

    printf("the number of bytes written is %d\n", ret_cnt);

    return 0;
}
