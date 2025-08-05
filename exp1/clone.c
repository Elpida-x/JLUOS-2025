#define _GNU_SOURCE
#include "sched.h"
#include <sys/types.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <sys/wait.h>
#include <string.h>

// 函数声明
int producer(void* args);
int consumer(void* args);

// 全局变量定义
pthread_mutex_t mutex;       // 互斥锁，保护临界区
sem_t product;               // 产品信号量，记录可用产品数量
sem_t warehouse;             // 仓库信号量，记录空闲位置数量

char buffer[8][4];           // 缓冲区数组，大小为8，每个元素占4字节
int bp = 0;                  // 缓冲区指针，指向当前空闲位置

int main(int argc, char** argv) {
    // 初始化同步机制
    pthread_mutex_init(&mutex, NULL);
    sem_init(&product, 0, 0);           // 初始产品数量为0
    sem_init(&warehouse, 0, 8);         // 初始空闲位置为8（缓冲区大小）

    int clone_flag, retval, arg;
    // 设置clone标志：共享内存、信号处理、文件系统和文件描述符
    clone_flag = CLONE_VM | CLONE_SIGHAND | CLONE_FS | CLONE_FILES;

    // 创建2个生产者和2个消费者线程
    for (int i = 0; i < 2; i++) {
        arg = i;  // 传递线程ID参数
        
        // 创建生产者线程
        char* stack1 = (char*)malloc(4096);  // 为线程分配独立栈空间
        // clone参数：函数指针、栈顶地址、标志位、参数
        retval = clone(producer, &(stack1[4095]), clone_flag | SIGCHLD, (void*)&arg);
        
        // 创建消费者线程
        char* stack2 = (char*)malloc(4096);
        retval = clone(consumer, &(stack2[4095]), clone_flag | SIGCHLD, (void*)&arg);
        
        usleep(1);  // 短暂延迟避免参数竞争问题
    }

    // 等待所有子线程结束
    int pid, status;
    for (int i = 0; i < 4; i++) {
        pid = wait(&status);
    }

    exit(0);
}

/**
 * 生产者线程函数
 * @param args 线程ID参数
 * @return 线程ID
 */
int producer(void* args) {
    int id = *((int*)args);  // 获取线程ID

    // 生产10个产品
    for (int i = 0; i < 10; i++) {
        sleep(i + 1);  // 模拟生产耗时，逐渐增加延迟
        
        sem_wait(&warehouse);          // 等待空闲位置（P操作）
        pthread_mutex_lock(&mutex);    // 加锁保护缓冲区指针

        // 根据线程ID生产不同产品
        if (id == 0)
            strcpy(buffer[bp], "aaa");
        else
            strcpy(buffer[bp], "bbb");
        
        // 打印生产信息
        printf("producer%d produce %s in %d\n", id, buffer[bp], bp);
        bp++;  // 移动缓冲区指针

        pthread_mutex_unlock(&mutex);  // 解锁
        sem_post(&product);            // 产品数量+1（V操作）
    }

    printf("producer %d is over!\n", id);
    return id;  // 返回线程ID，不使用exit避免终止整个进程
}

/**
 * 消费者线程函数
 * @param args 线程ID参数
 * @return 线程ID
 */
int consumer(void* args) {
    int id = *((int*)args);  // 获取线程ID

    // 消费10个产品
    for (int i = 0; i < 10; i++) {
        sleep(10 - i);  // 模拟消费耗时，逐渐减少延迟
        
        sem_wait(&product);           // 等待可用产品（P操作）
        pthread_mutex_lock(&mutex);   // 加锁保护缓冲区指针

        bp--;  // 移动缓冲区指针
        // 打印消费信息
        printf("consumer%d get %s in %d\n", id, buffer[bp], bp);
        strcpy(buffer[bp], "zzz");    // 清空已消费位置

        pthread_mutex_unlock(&mutex);  // 解锁
        sem_post(&warehouse);          // 空闲位置+1（V操作）
    }

    printf("consumer %d is over!\n", id);
    return id;  // 返回线程ID
}
