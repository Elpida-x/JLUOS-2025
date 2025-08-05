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

int producer(void *args);
int consumer(void *args);
pthread_mutex_t mutex;//互斥锁
sem_t product;//物品（信号量）
sem_t warehouse;//空位

char buffer[8][4];
int bp = 0;//指向当前空位

int main(int argc, char **argv) {
	pthread_mutex_init(&mutex, NULL);
	sem_init(&product, 0, 0);
	sem_init(&warehouse, 0, 8); // 缓冲区大小为8

	int clone_flag,retval,arg;
	clone_flag = CLONE_VM | CLONE_SIGHAND | CLONE_FS | CLONE_FILES;
	//创建4个线程
	for (int i = 0; i < 2; i++) {
		arg = i;        
		// 创建生产者线程
		char *stack1 = (char *)malloc(4096);//独立的栈
		/* clone创建的新进程转去调用参数fn所指定的函数 */
		retval = clone(producer, &(stack1[4095]), clone_flag | SIGCHLD, (void *)&arg);
		// 创建消费者线程
		char *stack2 = (char *)malloc(4096);
		retval = clone(consumer, &(stack2[4095]), clone_flag | SIGCHLD, (void *)&arg);
                /* &(stack1[4095]) 在Linux系统中，栈是从高地址向低地址增长的，而&(stack1[4095])指向这块分配内存的高地址端，也就是栈顶位置。*/
		usleep(1); //稍作延迟避免参数竞争
	}

	int pid, status;
	for (int i = 0; i < 4; i++) {
		pid = wait(&status);
	}

	exit(0);//exit(1);
}

int producer(void *args) {
	int id = *((int *)args);

	for (int i = 0; i < 10; i++) {
		sleep(i + 1); //模拟不同速度
		sem_wait(&warehouse);//若无空位，需等待
		pthread_mutex_lock(&mutex);//加锁，实现临界区互斥 bp
		
		if (id == 0)
			strcpy(buffer[bp], "aaa\0");
		else
			strcpy(buffer[bp], "bbb\0");
		printf("producer%d produce %s in %d\n", id, buffer[bp], bp);
		bp++;

		pthread_mutex_unlock(&mutex);//解锁
		sem_post(&product);//+1,唤醒消费者取物
	}

	printf("producer %d is over!\n", id);
	return id;//不使用 exit(), exit will cancel the whole process
}

int consumer(void *args) {
	int id = *((int *)args);

	for (int i = 0; i < 10; i++) {
		sleep(10 - i); // 模拟不同速度
		sem_wait(&product);//若无物品，需等待
		pthread_mutex_lock(&mutex);

		bp--;
		printf("consumer%d get %s in %d\n", id, buffer[bp], bp);
		strcpy(buffer[bp], "zzz\0");

		pthread_mutex_unlock(&mutex);
		sem_post(&warehouse);
	}

	printf("consumer %d is over!\n", id);
	return id;
}
