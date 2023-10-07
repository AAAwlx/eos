#include<stdio.h>
int e=0;
int aaa() {
    e++;
    printf("aaa\n");
    if(e==10){
        return 1;
    }
    main(1, 2, 3, 4);
}
int main(int a,int b,int c,int d)
{
    
    printf("%d %d %d %d |\n",a , b, c,d);
    if(aaa()==1){
        return;
    }
}