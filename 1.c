#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define SIZE 10

// 全局变量，用于存储分配的内存地址
int* sharedMemory;

void* threadA_function(void* arg) {
    // 线程A分配内存
    sharedMemory = (int*)malloc(SIZE * sizeof(int));
    
    // 在内存中存储一些数据
    for (int i = 0; i < SIZE; ++i) {
        sharedMemory[i] = i;
    }

    return NULL;
}

void* threadB_function(void* arg) {
    // 线程B访问内存中的数据
    for (int i = 0; i < SIZE; ++i) {
        printf("Thread B: %d\n", sharedMemory[i]);
    }

    return NULL;
}

int main() {
    pthread_t threadA, threadB;

    // 创建线程A和线程B
    pthread_create(&threadA, NULL, threadA_function, NULL);
    pthread_create(&threadB, NULL, threadB_function, NULL);

    // 等待线程A和线程B完成
    pthread_join(threadA, NULL);
    pthread_join(threadB, NULL);

    // 释放内存
    free(sharedMemory);

    return 0;
}
