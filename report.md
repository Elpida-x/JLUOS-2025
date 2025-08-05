# 操作系统实验报告

虚拟机OS型号：CentOS7

## 实验一 进程与线程——Linux进程与线程通信

### 实验内容

1. 以Linux系统进程和线程机制为背景，掌握fork()和clone()系统调用的形式和功能，以及与其相适应的高级通讯方式，由fork派生的子进程之间通过pipe通讯，由clone创建的线程之间通过共享内存通讯，后者需要考虑互斥问题。

2. 以生产者/消费者问题为例，通过实验理解fork()和clone()两个系统调用的区别。程序要求能够创建4个进程或线程，生产者和消费者之间能够传递数据。

### 实验准备

1. 管道是一种最基本的IPC机制，作用于有血缘关系的进程之间，完成数据传递。调用pipe系统函数即可创建一个管道。有如下特质：
   1. 其本质是一个**伪文件**(实为内核缓冲区)。
   2. 由两个文件描述符引用，一个表示读端，一个表示写端。
   3. 规定数据从管道的写端流入管道，从读端流出。
   4. **管道的read和write系统调用是原子操作**，确保单字节读写不会被中断，避免竞态条件。

2. 管道的局限性：
   1. 数据自己读不能自己写。
   2. 数据一旦被读走，便不在管道中存在，不可反复读取。
   3. 由于管道采用半双工通信方式。因此，数据只能在一个方向上流动。
   4. **只能在有公共祖先的进程间使用管道**。
3. `write(fd,buf,count)` 会把参数buf所指的内存写入count个字节到参数fd所指的文件内。
返回值：如果顺利write()会返回实际写入的字节数（len），当有错误发生时则返回-1.

4. `read(fd,buf,count)`会把参数fd所指的文件传送count个字节到buf指针所指的内存中。返回值为实际读取到的字节数，返回0代表已读到文件尾或无可读数据。

5. 0号进程是系统启动时由内核创建的第一个进程，负责初始化和管理其他进程，并在没有其他任务时占用CPU 空闲时间，0号进程直接生成1号进程，1号进程时所有用户进程的祖先。

6. **进程间的通信方式**：管道通信、消息队列、共享内存、信号量、信号、套接字、mmap

7. 线程共享进程的代码段和数据段，但是**线程有各自的用户栈**，可以独立调用，占用CPU运行。

8. P操作：`s.value--; if(s.value<0)then asleep(s.queue);` 小于等于0都需要等待。
   V操作：`s.value++; if(s.value<=0)then wakeup(s.queue);` s>0表示当前仍有可用的资源，无需唤醒其他进程；s<=0表示有进程因为等待该资源而堵塞，需要从等待队列中唤醒一个进程，使其进入就绪队列中。 

9. **用户级别线程**基于library函数，系统不可见，TCB在用户空间，每个进程一个系统栈。同一进程中多线程切换速度快（不需要进入操作系统）。不能真正并行，一个线程受阻，其他也不能执行。
   **核心级别线程**由操作系统创建，线程是CPU调度的基本单位，TCB在系统空间，每个线程一个系统栈。多线程可以真正并行，线程状态转换需要进入操作系统，系统开销大。

10. `clone`函数创建的是轻进程（不等于线程）。


### 实验设计

#### 进程通信 & fork

```c
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "sys/wait.h"
#include "sys/types.h"
#include "sys/file.h"
#include "unistd.h"

char r_buf[4];  //读缓冲区
char w_buf[4];  //写缓冲区
int pipe_fd[2]; //文件描述符
pid_t pid1, pid2, pid3, pid4; //定义进程的pid(pid_t==int)

int producer(int id);
int consumer(int id);

int main(int argc,char **argv) 
{  
	if(pipe(pipe_fd)<0) //first 创建管道，返回文件描述符
	{
		printf("pipe create error\n");
		exit(-1);
    }
	else
    {
		printf("pipe %d is created successfully!\n", getpid());
		if((pid1=fork())==0)//fork的子进程继续运行时以调用处为起点
			producer(1);
		if((pid2=fork())==0)//父子进程的返回值不同
	   		producer(2);
		if((pid3=fork())==0)//动态、并发
		   consumer(1);
		if((pid4=fork())==0)
		   consumer(2);
    }

	close(pipe_fd[0]);  //需要加上这两句
	close(pipe_fd[1]);  //否这会有读者或者写者永远等待
    
    /* 进程同步: fork调用成功后，父子进程各做各的事情，父进程等待子进程运行结束*/
	int pid,status;
	for(int i=0;i<4;i++){
    	pid=wait(&status);//等待子进程终止
	}
      
	exit(0);
}

int producer(int id)
{
	printf("producer %d is running!\n",id);
	close(pipe_fd[0]); //生产者往管道中写数据（放入物品）
	for(int i=1;i<10;i++)
	{
		sleep(3);
		if(id==1) //producer 1
			strcpy(w_buf,"aaa\0");
		else  //producer 2
			strcpy(w_buf,"bbb\0");
		if(write(pipe_fd[1],w_buf,4)==-1)
			printf("write to pipe error\n");
	}
	close(pipe_fd[1]);
	printf("producer %d is over!\n",id);
	exit(id);//exit(status)将子进程的状态信息返回给父进程
}

int consumer(int id)
{
	close(pipe_fd[1]); //消费者从管道中读数据（取出物品）
	printf("consumer %d is running!\n",id);

	//证明: 当一个进程改变其空间数据时，其他进程空间对应数据内容并未改变
	if (id==1)  //消费者1
		strcpy(w_buf,"ccc\0");
	else  //消费者2
		strcpy(w_buf,"ddd\0");

	while(1)
	{
		sleep(1);
		strcpy(r_buf,"eee\0"); //init
		if(read(pipe_fd[0],r_buf,4)==0) //return 0 means read to end
		   break;     
		printf("consumer %d get %s, while the w_buf is %s\n",id,r_buf,w_buf);
	}
	close(pipe_fd[0]);
	printf("consumer %d is over!\n", id);
	exit(id); //进程自我终结
}
```

>编译：gcc fork.c -o fork.out 源文件：fork.c
 运行：./fork.out 编译后生成文件：fork.out
 
 运行结果：
 ![fork函数运行结果](image-1.png)

#### 线程通信 & clone
```c
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
```

>编译：gcc clone.c -o clone.out -pthread
 运行：./clone.out

运行结果：
![clone结果](image-2.png)


### 思考总结

**1.进程与线程在通信方式上的区别？**
进程采用管道进行通信，子进程继承父进程的管道，管道对外匿名，只能在有公共祖先的进程间使用管道，非家族进程不能通过管道通信。
线程通过共享内存通信，多个线程映射同一块物理内存，通信效率高，但需自行处理同步（如信号量）。

**2.操作系统实现互斥的方式有哪些？**
(1) 禁止中断：在单核系统中，通过临时关闭中断来阻止进程/线程切换，确保临界区代码独占执行。
(2) 软件实现方法：Dekker算法、Peterson算法、Lamport面包店算法、Eisenberg/Mcguire算法。
(3) 信号量与PV操作
```
sem_t sem;
sem_init(&sem, 0, 1); // 二进制信号量（互斥锁）
sem_wait(&sem);       // P 操作（进入临界区）
sem_post(&sem);       // V 操作（离开临界区）
```
(4) 互斥锁：操作系统提供锁，获取失败时阻塞线程。
```
pthread_mutex_t mutex;
pthread_mutex_init(&mutex,NULL); // 初始化
pthread_mutex_lock(&mutex);      // 阻塞直到获取锁
pthread_mutex_unlock(&mutex);    // 释放锁
```
(5) 管程
管程是一种特殊的**数据类型**，它**将共享变量以及对共享变量的操作封装到一起**，是一种集中式的同步机制。管程的共享变量在管程外部不可见，外部只能通过调用管程中的子程序访问共享变量。进程互斥地通过调用内部过程进入管程，当某进程在管程内执行时，其他想进入管程的进程必须等待。

进入管程：申请管程互斥权；
离开管程：如紧急等待队列非空，唤醒第一个等待者，否则开放管程，允许入口等待队列的一个进程进入管程。

![管程形式、语义与操作](管程知识点-1.jpg)

**3.管道open、close改变的是什么？**
联系文件系统的u_ofile表、内存file表、内存inode表（第八章文件系统PPT：UNIX创建管道文件）。

(1) 用户打开文件表（u_ofile表）
每个进程拥有自己的u_ofile表，记录进程打开的文件描述符对应的file结构指针。
父进程或子进程调用close(fd)时，会清除u_ofile中对应的索引，使其**不再指向内存file表**，关闭操作仅影响当前进程的u_ofile.

(2) 内存file表
整个系统一个，每个 file 结构记录文件的打开模式、偏移量、引用计数等信息。
读端与写端各一个file结构，每次**close(fd)会减少对应引用计数(f_count)**。
读端 (fd[0])：若父进程和子进程均关闭读端，则读端 file 结构的 f_count 减至 0，系统回收该结构。写端 (fd[1]) 类似读端。

(3) 内存inode表
当**读端和写端的所有file结构均被释放（即 f_count 均为 0）**，系统检查 inode 的引用计数（i_count）。若 i_count 减至 0，释放内存中的 inode 结构，**管道资源被回收**。

---
## 实验二 处理机调度——实时调度算法EDF和RMS

### 实验内容

在Linux环境中采用用户级线程模拟实现EDF和RMS两种实时调度算法。给定一组实时任务，按照EDF算法和RMS算法分别判断是否可调度，在可调度的情况下，创建一组用户级线程，分别代表各个实时任务，并按算法确定的调度次序安排各个线程运行，运行时在终端上画出其Gantt图。为避免图形绘制冲淡算法，Gantt图可用字符表示。

### 实验准备

1. EDF算法和RMS算法的可调度条件及调度原则。
   **EDF**是可抢先式调度算法，调度原则是 $$ \sum_{i=1}^n \frac{C_i}{T_i} \leq 1 $$
   **RMS**是不可抢先调度算法，调度原则是 $$ \sum_{i=1}^n \frac{C_i}{T_i} \leq n(2^{1/n} - 1) $$

1. 在Linux环境中创建用户级线程的函数。
```c
int pthread_create (pthread_t *THREAD,               //pthread_t型的指针，用于保存线程id
                    pthread_attr_t * ATTR,           //说明要创建的线程的属性
                    void * (*START_ROUTINE)(void *), //指明线程的入口
                    void * ARG)                      //传给线程入口函数的参数
```

### 实验设计
实时任务用task数据结构描述，设计四个函数：select_proc()用于实现调度算法，被选中任务执行proc()，在没有可执行任务时执行idle()，主函数mian()初始化相关数据，创建实时任务并对任务进行调度。
为模拟调度算法，给每个线程设置一个等待锁，暂不运行的任务等待在相应的锁变量上。主线程按调度算法唤醒一个子线程，被选中线程执行一个时间单位，然后将控制权交给主线程判断是否需要重新调度。


```c
#include "math.h"
#include "sched.h"
#include "pthread.h"
#include "stdlib.h"
#include "semaphore.h" 
#include "stdio.h"
#include <unistd.h>

typedef struct{   //实时任务描述
    char task_id; //任务标识符
    int call_num; //任务调用次数
    int ci;       //任务计算时间
    int ti;       //任务周期时间 
    int ci_left;  //剩余计算时间
    int ti_left;  //剩余周期时间
    int flag;     //任务是否活跃，0否，2是
    int arg;      //序号
    pthread_t th; //任务对应线程
}task;

void proc(int* args);
void* idle();
int select_proc();

int task_num = 0;   //实时任务数
int idle_num = 0;   //闲逛任务执行次数

int alg;            //所选算法，1 for EDF，2 for RMS
int curr_proc = -1; //当前被选中的任务索引
int demo_time = 100;//演示时间(时间单位的长度)

task* tasks;                          //动态分配
pthread_mutex_t proc_wait[100];       //保证线程之间的互斥
pthread_mutex_t main_wait, idle_wait; //主线程管理其他线程
float sum=0;                          //用于判断是否可调度
pthread_t idle_proc;                  //闲逛进程对应的线程

int main(int argc,char** argv)
{   
	pthread_mutex_init(&main_wait,NULL);
	pthread_mutex_lock(&main_wait);  //主线程锁住自己，等待解锁
	pthread_mutex_init(&idle_wait,NULL);
	pthread_mutex_lock(&idle_wait);  //下次执行lock等待

	printf("Please input number of real time tasks:\n");
	scanf("%d",&task_num);
	tasks = (task*)malloc(task_num*sizeof(task));
	int i;
	for(i=0;i<task_num;i++)
	{
		pthread_mutex_init(&proc_wait[i],NULL);
		pthread_mutex_lock(&proc_wait[i]); //每个任务初始等待调度
	}
	for(i=0;i<task_num;i++)
	{
		printf("Please input task id, followed by Ci and Ti:\n");
		getchar();
		scanf("%c %d %d",&tasks[i].task_id,&tasks[i].ci,&tasks[i].ti);
		tasks[i].ci_left=tasks[i].ci;
		tasks[i].ti_left=tasks[i].ti;
		tasks[i].flag=2;
		tasks[i].arg=i;
		tasks[i].call_num=1; 
		sum=sum+(float)tasks[i].ci/(float)tasks[i].ti;
	}

	printf("Please input algorithm, 1 for EDF, 2 for RMS:");
	scanf("%d",&alg);
	printf("Please input demo time:");
	scanf("%d",&demo_time);
	double r=1;  //默认EDF算法
	if(alg==2)
	{  //RMS算法
		r=((double)task_num)*(exp(log(2)/(double)task_num)-1);
		printf("r is %lf\n",r);
	}
	if(sum>r)
	{  //不可调度
		printf("(sum=%lf > r=%lf) ,not schedulable!\n",sum,r);
		exit(2);
	}

	pthread_create(&idle_proc,NULL,(void*)idle,NULL); //创建闲逛线程
	for(i=0;i<task_num;i++)  //创建实时任务线程
		pthread_create(&tasks[i].th,NULL,(void*)proc,&tasks[i].arg);

	/*每运行一个时间单位就将控制权交给主线程，判断是否需要切换实时任务，可以看作发生了一次时钟中断。*/
	for(i=0;i<demo_time;i++)
	{
		int j; 
		if((curr_proc=select_proc(alg))!=-1)
		{  //按调度算法选线程
			pthread_mutex_unlock(&proc_wait[curr_proc]);  //唤醒
			pthread_mutex_lock(&main_wait);  //主线程等待
		}else
		{   //无可运行任务，选择闲逛线程
			 pthread_mutex_unlock(&idle_wait);  
			 pthread_mutex_lock(&main_wait);
		}
		for(j=0;j<task_num;j++)
		{  //Ti--，为0时开始下一周期      
			if((--tasks[j].ti_left)==0){
				 tasks[j].ti_left=tasks[j].ti;
				 tasks[j].ci_left=tasks[j].ci;
				 pthread_create(&tasks[j].th,NULL,(void*)proc,&tasks[j].arg);
				 tasks[j].flag=2;
			}
		}
	}
	printf("\n");
	sleep(10);
	exit(0);
};

void proc(int* args)
{
	while(tasks[*args].ci_left>0)//当前任务还没执行完本次的process时间
	{
		pthread_mutex_lock(&proc_wait[*args]);  //等待被调度
		if(idle_num!=0){
			printf("idle(%d)",idle_num);
			idle_num=0;
		}
		printf("%c%d",tasks[*args].task_id,tasks[*args].call_num);        
		tasks[*args].ci_left--;  //执行一个时间单位
		if(tasks[*args].ci_left==0){
			printf("(%d)",tasks[*args].ci);
			tasks[*args].flag=0;
			tasks[*args].call_num++;
		}
		pthread_mutex_unlock(&main_wait); //唤醒主线程
	}
};

void* idle()
{
	while(1)
	{
		pthread_mutex_lock(&idle_wait);  //等待被调度
		printf("->");  //空耗1个时间单位
		idle_num++;
		pthread_mutex_unlock(&main_wait);  //唤醒主控线程
	}
};

int select_proc(int alg)
{
	int j;
	int temp1,temp2;
	temp1=10000;
	temp2=-1;
	if((alg==2)&&(curr_proc!=-1)&&(tasks[curr_proc].flag!=0))
		return curr_proc; 

	for(j=0;j<task_num;j++)
	{ //遍历所有task，找剩余时间/周期最小的那个任务
		if(tasks[j].flag==2)
		{
			switch(alg){
			case 1:    //EDF算法
				if(temp1>tasks[j].ti_left){ //最短剩余时间优先
					temp1=tasks[j].ti_left;
					temp2=j;
				}
				break;
			case 2:    //RMS算法
				if(temp1>tasks[j].ti){      //最短周期时间优先
					temp1=tasks[j].ti;
					temp2=j;
				}
				break;
			}//switch	
		}//if
	}//for
   return temp2;
}
```

> 编译：gcc schedule.c -o schedule.out -lm -lpthread
> 运行：./schedule.out 

运行结果：
![EDF调度2任务](image-3.png)
![RMS调度3任务](image-4.png)
![alt text](image-6.png)

### 思考总结

1.上述参考算法中，被选中任务每运行一个时间单位即将控制权交给主线程，判断是否需要切换实时任务，这可看作发生一次时钟中断。实际上时钟中断的发生频率远没有这样频繁，因而上述调度会产生较大的开销。改进上述算法，使其只在需要重新调度任务时才返回主控线程。

改进策略：
思路1：增大两次调度的时间片间隔(实现起来较为复杂，main、proc、idle函数都需要改) 
思路2：预测下一次调度的结果，如果一样则不进行切换

```c
for(i=0;i<demo_time;i++){
	int j;
	int next_proc=select_proc(alg);//预测调度结构
	if(curr_proc!=next_proc)
	{
		if((curr_proc=next_proc)!=-1){  
			pthread_mutex_unlock(&proc_wait[curr_proc]);
			pthread_mutex_lock(&main_wait);  
		}else{   
			pthread_mutex_unlock(&idle_wait);  
			pthread_mutex_lock(&main_wait);
		}
	}
	/*余下函数保持一致*/
}
```

2.在上述改进的基础上，对一组可调度实时事务，统计对不同调度算法的线程切换次数（不计主线程切换），并将其显示出来。

设置全局变量 EDFtimes 和 RMStimes 分别记录两种算法的线程切换次数。
初值设置为0，在每次调度时进行累加计数。

```c
int select_proc(int alg){
	/*其余代码不变*/
	switch(alg){
	case 1:    //EDF算法
		EDFtimes++;
		if(temp1>tasks[j].ti_left){ //最短剩余时间优先
			temp1=tasks[j].ti_left;
			temp2=j;
		} break;
	case 2:    //RMS算法
		RMStimes++;
		if(temp1>tasks[j].ti){      //最短周期时间优先
			temp1=tasks[j].ti;
			temp2=j;
		} break;
	}//switch
	/*其余代码不变*/
}
```


---
## 实验四 文件系统——散列结构文件

### 实验内容

1. 参考教材中hash文件构造算法，设计一组hash文件函数，包括hash文件创建、打开、关闭、读、写等。
2. 编写一个测试程序，通过记录保存、查找、删除等操作，检查上述hash文件是否实现相关功能。

### 实验准备

1. 散列文件核心算法，包括记录保存、记录查找、记录删除等。
   ![散列算法](115aa19400ea8b9329de8b5eb81c2acc_720.jpg)


2. Linux系统有关文件的系统调用命令：**creat、open、close、read、write、seek**。哈希文件构造是基于这些系统调用命令实现的。
   
   * ```int creat(const char* pachname, mode_t mode);```
	成功返回为只写打开的文件描述符，失败返回-1
	mode表示文件权限模式
	例：0666  (4r 2w 1x 0-)

   * ```int open(const char* pathname, int flags, mode_t mode);```
	pathname：文件路径
	flag：标志位
	O_RDONLY(只读)、O_WRONLY(只写)、O_RDWR(读写)、O_CREAT(检测文件是否存在，若不存在则新建)
	

   * ```int close(int fd);```
	关闭文件，成功返回0，失败返回-1

   * ```ssize_t write(int fd, const void* buf, size_t count);```
	把buf缓冲区中的写count个字节到fd指向的文件中

   * ```ssize_t read(int fd, const void* buf, size_t count);```
	把fd指向的文件中的内容读count个字节到buf缓冲区中

   * ```off_t lseek(int fd, off_t offset, int whence)```
	成功则返回当前位置到开始的长度，失败则返回-1并设置errno
	whence：位置 
	SEEK_SET文件头，SEEK_CUR当前位置，SEEK_END文件尾
	offset为正时，fd从whence的位置出发向前移动offset个长度，offset为负时移动方向相反。

**Hash文件结构：**

```
文件整体结构
0                     16                     48                     80
┌─────────────────────┬─────────────────────┬─────────────────────┬───
│  HashFileHeader     │ 槽位0               │ 槽位1               │ ...
│  16字节             ├─────────┬───────────┼─────────┬───────────┤
│                     │ CFTag   │ jtRecord  │ CFTag   │ jtRecord  │
│                     │ 2字节   │ 32字节     │ 2字节   │ 32字节    │
└─────────────────────┴─────────┴───────────┴─────────┴───────────┴───

单个槽位结构(34字节):
0        2        34
┌────────┬───────────────┐
│ CFTag  │   jtRecord    │
├───┬────┼───────┬───────┤
│c  │f   │ key   │ other │
└───┴────┴───────┴───────┘
```

 

### 实验设计

对实验内容和函数的理解放在了代码注释里。

#### Hash文件
HashFile.h
```c
#ifndef HASHFILE_H
#define HASHFILE_H
 
#include<unistd.h>
#include<sys/stat.h>
#define COLLISIONFACTOR 0.5

struct HashFileHeader{
	int sig;             // 文件标识(固定值31415926)
	int reclen;          // 单条记录长度
	int total_rec_num;   // 哈希表总容量
	int current_rec_num; // 当前记录数
}; //文件头结构
 
struct CFTag{
	char collision; // 冲突计数
	char free;      // 空闲标志(0空闲，-1占用)
};//每条记录的标记头，存储记录状态

//创建一个哈希文件，返回文件描述符，若创建失败则返回-1
int hashfile_creat(const char* filename, mode_t mode, int reclen, int total_rec_num);

//打开一个哈希文件，返回文件描述符，若打开失败则返回-1
int hashfile_open(const char* filename, int flags, mode_t mode);

//关闭一个哈希文件，返回0，若关闭失败则返回-1
int hashfile_close(int fd);

//按key读取哈希文件中的一条记录，若失败则返回-1
int hashfile_read(int fd, int keyoffset, int keylen, void* buf);

//向哈希文件中写入一条记录（只是调用hashfile_saverec函数），若失败则返回-1
int hashfile_write(int fd, int keyoffset, int keylen, void* buf);

//删除哈希文件中buf对应的记录，若删除成功则返回0，若失败则返回-1
int hashfile_delrec(int fd, int keyoffset, int keylen, void* buf);

//查找哈希文件中buf对应的记录，若找到则返回记录的偏移量，若未找到则返回-1
int hashfile_findrec(int fd, int keyoffset, int keylen, void* buf);

//将一条记录保存到哈希文件中，返回写入的字节数，若失败则返回-1
int hashfile_saverec(int fd, int keyoffset, int keylen, void* buf);

//计算哈希值，返回哈希地址
int hash(int keyoffset, int keylen, void* buf, int total_rec_num);

//检查哈希文件是否已满，返回0表示未满，返回1表示已满
int checkHashFileFull(int fd);

//读取哈希文件头信息，返回读取的字节数，若失败则返回-1
int readHashFileHeader(int fd, struct HashFileHeader* hfh);

#endif
```

HashFile.c
```c
#include<unistd.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<stdio.h>
#include<stdlib.h>
#include<memory.h>
#include"HashFile.h"
 
int hashfile_creat(const char* filename, mode_t mode, int reclen, int total_rec_num)
{
	//初始化哈希头
	struct HashFileHeader hfh;
	hfh.sig = 31415926;
	hfh.reclen = reclen;
	hfh.total_rec_num = total_rec_num;
	hfh.current_rec_num = 0;

	int fd;
	int rtn;
	char* buf;
	//fd = open(filename,mode);
	fd = creat(filename, mode);//以mode权限创建文件，返回文件描述符
	if (fd != -1)
	{
		rtn = write(fd, &hfh, sizeof(struct HashFileHeader));//把文件头写到哈希文件中
		if (rtn != -1)
		{
			//写入空闲标志：单位槽大小=reclen+sizeof(CFTag)，总槽位数=total_rec_num.
			buf = (char*)malloc((reclen + sizeof(struct CFTag)) * total_rec_num);
			//分配内存并清零：将buf指向的内存区域的所有字节设置为0，collision=0，free=0.
			memset(buf, 0, (reclen + sizeof(struct CFTag)) * total_rec_num);
			//把记录内容写入文件
			rtn = write(fd, buf, (reclen + sizeof(struct CFTag)) * total_rec_num);
			free(buf);
		}
		close(fd);//确保系统资源及时释放，保证数据完整性 ***
		return fd;
	}
	else { //文件创建失败
		close(fd);
		return -1;
	}
}

int hashfile_open(const char* filename, int flags, mode_t mode)
{
	int fd = open(filename, flags, mode);//(文件路径,标志位,文件权限)
	struct HashFileHeader hfh;
	if (read(fd, &hfh, sizeof(struct HashFileHeader)) != -1) { //读取文件头
		lseek(fd, 0, SEEK_SET);  //将fd移到文件开头，保证文件打开后"可从头操作"
		if (hfh.sig == 31415926) //验证文件标识
			return fd;
		else return -1;
	}
	else return -1;
}

int hashfile_close(int fd)
{
	return close(fd);
}

int hashfile_read(int fd, int keyoffset, int keylen, void* buf)
{
	struct HashFileHeader hfh;
	readHashFileHeader(fd, &hfh); //读取文件头信息
	int offset = hashfile_findrec(fd, keyoffset, keylen, buf);
	if (offset != -1) {
		lseek(fd, offset + sizeof(struct CFTag), SEEK_SET);//定位到记录起始位置
		return read(fd, buf, hfh.reclen);
	}
	else {
		return -1;
	}
}

int hashfile_write(int fd, int keyoffset, int keylen, void* buf)
{
	return hashfile_saverec(fd, keyoffset, keylen, buf);
	//return -1;
}

int hashfile_delrec(int fd, int keyoffset, int keylen, void* buf)
{
	int offset;
	offset = hashfile_findrec(fd, keyoffset, keylen, buf);
	if (offset != -1)
	{
		struct CFTag tag;
		read(fd, &tag, sizeof(struct CFTag));
		tag.free = 0; //置空闲标志 
		lseek(fd, offset, SEEK_SET);
		write(fd, &tag, sizeof(struct CFTag));
		struct HashFileHeader hfh;
		readHashFileHeader(fd, &hfh);
		int addr = hash(keyoffset, keylen, buf, hfh.total_rec_num);
		offset = sizeof(struct HashFileHeader) + addr * (hfh.reclen + sizeof(struct CFTag));
		if (lseek(fd, offset, SEEK_SET) == -1)
			return -1;
		read(fd, &tag, sizeof(struct CFTag));
		tag.collision--; //hash索引位置的冲突计数减一

		lseek(fd, offset, SEEK_SET);
		write(fd, &tag, sizeof(struct CFTag));
		hfh.current_rec_num--; //当前记录数减1
		lseek(fd, 0, SEEK_SET);
		write(fd, &hfh, sizeof(struct HashFileHeader));//更新文件记录数
	}
	else {
		return -1;
	}
}
 
int hashfile_findrec(int fd, int keyoffset, int keylen, void* buf)
{
	struct HashFileHeader hfh;
	readHashFileHeader(fd, &hfh);
	int addr = hash(keyoffset, keylen, buf, hfh.total_rec_num);
	int offset = sizeof(struct HashFileHeader) + addr * (hfh.reclen + sizeof(struct CFTag));
	if (lseek(fd, offset, SEEK_SET) == -1)
		return -1;
	
	struct CFTag tag;
	read(fd, &tag, sizeof(struct CFTag));
	char count = tag.collision;//取addr对应记录的冲突计数
	if (count == 0)
		return -1; //不存在

recfree:
	if (tag.free == 0) //本记录空闲
	{
		offset += hfh.reclen + sizeof(struct CFTag);
		if (lseek(fd, offset, SEEK_SET) == -1)
			return -1;
		read(fd, &tag, sizeof(struct CFTag));//顺取下一记录
		goto recfree;
	}
	else //本记录被占用
	{
		char* p = (char*)malloc(hfh.reclen * sizeof(char));//创建临时缓冲区读取文件记录
		read(fd, p, hfh.reclen);
		char* p1 = (char*)buf + keyoffset;	//目标键值位置
		char* p2 = p + keyoffset;		//当前文件记录键值位置

		if (memcmp(p1, p2, keylen) == 0) {     //匹配成功
			free(p);
			p = NULL;
			return(offset);//返回记录位置
		}
		else { //如果key不匹配，判断该记录是否属于同一个哈希槽（防止跨槽误判）
			if (addr == hash(keyoffset, keylen, p, hfh.total_rec_num)) {
				count--;
				if (count == 0) { //所有冲突记录都检查过了
					free(p);
					p = NULL;
					return -1; //不存在 
				}
			}
			free(p);
			p = NULL;
			offset += hfh.reclen + sizeof(struct CFTag);
			if (lseek(fd, offset, SEEK_SET) == -1)
				return -1;
			read(fd, &tag, sizeof(struct CFTag)); //顺取下一记录
			goto recfree;
		}
	}
}

int hashfile_saverec(int fd, int keyoffset, int keylen, void* buf)
{
	//检查哈希文件是否已满
	if (checkHashFileFull(fd))
		return -1;
	struct HashFileHeader hfh;
	readHashFileHeader(fd, &hfh);
	//计算哈希地址
	int addr = hash(keyoffset, keylen, buf, hfh.total_rec_num);
	int offset = sizeof(struct HashFileHeader) + addr * (hfh.reclen + sizeof(struct CFTag));//偏移量
	if (lseek(fd, offset, SEEK_SET) == -1)//从文件开头定位到指定位置offset
		return -1;
	
	struct CFTag tag;
	read(fd, &tag, sizeof(struct CFTag)); //从fd中offset位置中读取tag信息
	tag.collision++;//冲突计数+1

	lseek(fd, sizeof(struct CFTag) * (-1), SEEK_CUR); //将文件指针回退到tag的起始位置
	write(fd, &tag, sizeof(struct CFTag)); //把tag最新状态写入文件(tag.collision有修改)

	while (tag.free != 0) //本记录不空闲，冲突，顺序探查
	{
		offset += (hfh.reclen + sizeof(struct CFTag));
		if (offset >= lseek(fd, 0, SEEK_END)) //如果偏移量超过文件末尾，则回到文件开头
			offset = sizeof(struct HashFileHeader);
		if (lseek(fd, offset, SEEK_SET) == -1)
			return -1;
		read(fd, &tag, sizeof(struct CFTag)); //读取下一个槽位的tag信息
	}
	
	//找到了空闲的槽位
	tag.free = -1;
	lseek(fd, sizeof(struct CFTag) * (-1), SEEK_CUR);
	write(fd, &tag, sizeof(struct CFTag)); //把 tag的最新状态 写入文件
	write(fd, buf, hfh.reclen);	//把 buf中的记录 写入文件
	hfh.current_rec_num++; //当前记录数+1
	lseek(fd, 0, SEEK_SET);
	return write(fd, &hfh, sizeof(struct HashFileHeader));//更新hfh的状态 
}


int hash(int keyoffset, int keylen, void* buf, int total_rec_num)
{   //使用累加取模的简单哈希算法
	char* p = (char*)buf + keyoffset;
	int addr = 0;
	for (int i = 0; i < keylen; i++) {
		addr += (int)(*p);
		p++;
	} //循环：把key的每个字节的ASCII值累加到addr变量上
	return addr % (int)(total_rec_num * COLLISIONFACTOR);
}
 
int checkHashFileFull(int fd)
{
	struct HashFileHeader hfh;
	readHashFileHeader(fd, &hfh);
	if (hfh.current_rec_num < hfh.total_rec_num)
		return 0;
	else
		return 1; //若哈希表满，则返回1，否则返回0
}

int readHashFileHeader(int fd, struct HashFileHeader* hfh)
{
	lseek(fd, 0, SEEK_SET);
	return read(fd, hfh, sizeof(struct HashFileHeader));
}
```

#### 测试文件
jtRecord.h
```c
#define RECORDLEN 32 //记录的长度固定为32字节
struct jtRecord
{
	int key;							 // 4字节键值
	char other[RECORDLEN - sizeof(int)]; // 28字节其他数据
};
#ifdef HAVE_CONFIG_H //条件编译
#include<config.h>
#endif
```

jtRecord.c
```c
#ifdef HAVE_CONFIG_H
#include<config.h>
#endif
#include<stdio.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<string.h>
#include"HashFile.h"
#include"jtRecord.h"

#define KEYOFFSET 0
#define KEYLEN sizeof(int)
#define FileNAME "jing.hash"

void showHashFile();

int main(int argc, char* argv[])
{
	struct jtRecord rec[6] = {
		{1,"jing"},{2,"wang"},{3,"li"},{4,"zhang"},{5,"qing"},{6,"yuan"}
	};
	for (int j = 0; j < 6; j++)
	{   //计算哈希地址
		printf("<%d,%d>\t", rec[j].key, hash(KEYOFFSET, KEYLEN, &rec[j], 6));
	}
    //创建哈希文件
	int fd = hashfile_creat(FileNAME, O_RDWR | O_CREAT, RECORDLEN, 6);//6为总记录数

	printf("\nOpen and Save Record...\n");
	fd = hashfile_open(FileNAME, O_RDWR, 0);
	for (int i = 0; i < 6; i++){
		hashfile_saverec(fd, KEYOFFSET, KEYLEN, &rec[i]);//把rec[i]中的信息保存到文件中
	}
	hashfile_close(fd);
	showHashFile();

	//Demo find Rec
	printf("\nFind Record...");
	fd = hashfile_open(FileNAME, O_RDWR, 0);
	int offset = hashfile_findrec(fd, KEYOFFSET, KEYLEN, &rec[4]);//找rec[4]在文件中的位置(偏移量)
	printf("\noffset is %d\n", offset);
	hashfile_close(fd);
	struct jtRecord jt;
	struct CFTag tag;
	fd = open(FileNAME, O_RDWR);
	lseek(fd, offset, SEEK_SET);
	read(fd, &tag, sizeof(struct CFTag));
	printf("Tag is <%d,%d>\t", tag.collision, tag.free);
	read(fd, &jt, sizeof(struct jtRecord));
	printf("Record is {%d,%s}\n", jt.key, jt.other);

	//Demo Delete Rec
	printf("\nDelete Record...");
	fd = hashfile_open(FileNAME, O_RDWR, 0);
	hashfile_delrec(fd, KEYOFFSET, KEYLEN, &rec[2]);
	hashfile_close(fd);
	showHashFile();

	//Demo Read
	fd = hashfile_open(FileNAME, O_RDWR, 0);
	char buf[32];
	memcpy(buf, &rec[1], KEYLEN);
	hashfile_read(fd, KEYOFFSET, KEYLEN, buf);
	printf("\nRead Record is {%d,%s}\n", ((struct jtRecord*)buf)->key, ((struct jtRecord*)buf)->other);
	hashfile_close(fd);

	//Demo Write
	printf("\nWrite Record...");
	fd = hashfile_open(FileNAME, O_RDWR, 0);
	hashfile_write(fd, KEYOFFSET, KEYLEN, &rec[3]);
	hashfile_close(fd);
	showHashFile();
	return 0;
}

void showHashFile()
{
	int fd;
	printf("\n");
	fd = open(FileNAME, O_RDWR); //以读写方式打开FileNAME文件
	lseek(fd, sizeof(struct HashFileHeader), SEEK_SET);//移动到第一条记录的位置
	struct jtRecord jt;
	struct CFTag tag;
	while (1){
		if (read(fd, &tag, sizeof(struct CFTag)) <= 0)
			break;
		printf("Tag is <%d,%d>\t", tag.collision, tag.free);//冲突数，空闲位
		if (read(fd, &jt, sizeof(struct jtRecord)) <= 0)
			break;
		printf("Record is {%d,%s}\n", jt.key, jt.other);//记录
	}
	close(fd);
}
```

使用 *su root* 命令转移到 root 下运行代码.
> 编译：gcc HashFile.c jtRecord.c -o main
> 运行：./main

运行结果：
![alt text](image-10.png)

### 思考总结

1. 上述Hash文件缺少记录更新操作，试利用已有函数，编写 hashfile_update() 函数以完善Hash文件功能。

我的思路：在不改变键值的基础上，直接找到记录位置，覆盖非键值部分，保持原有冲突计数不变。

代码实现：
```c
int hashfile_update(int fd, int keyoffset, int keylen, void* buf)
{
	struct HashFileHeader hfh;
	//读取文件头
	if (readHashFileHeader(fd, &hfh) == -1) return -1;
	//查找目标记录在文件内的起始偏移
	int offset = hashfile_findrec(fd, keyoffset, keylen, buf);
	if (offset == -1) return -1; // 记录未找到
	// 定位到record区，跳过CFTag
	offset += sizeof(struct CFTag);
	if (lseek(fd, offset, SEEK_SET) == -1) return -1;
	// 将 buf 中的新记录内容写入文件
	int written = write(fd, buf, hfh.reclen);
	return written;
}
```
测试update功能：
```c
//Demo Update
printf("\nUpdate Record...");
int fd = hashfile_open("jing.hash", O_RDWR, 0);
struct jtRecord newrec;
newrec.key = 6; //新纪录的key=6，会覆盖原来的记录
strcpy(newrec.other, "updated");
int ret = hashfile_update(fd, KEYOFFSET, KEYLEN, &newrec);
if (ret ！= -1) printf("\nUpdate successfully!");
hashfile_close(fd);
showHashFile();
```
运行结果：原来 key=6 对应记录的内容由 "yuan" 改为 "updated".

![update功能](image-11.png)

2. 本实验所实现的hash文件是在Linux核心之外构造的，可以头文件或库函数的形式提供给其它应用程序。如果在核心级别上实现上述功能，应如何做？有何困难？

(1) 应如何做：
   i. 设计文件系统布局／设备接口；
   ii. 替换掉原来用户态的 open/read/write/malloc 等操作，改用内核接口；
   iii. 处理并发；
   iv. 确定内存分配时机与上下文；

(2)有何困难：
   i. 环境不同：没有标准库，内存分配复杂；
   ii. 要手动保证更新后的数据及时写回；
   iii. 调试成本高：内核模块一旦崩溃会产生很大的代价；
   iv. 存在安全性问题.

3. 遇到的问题

一开始在普通用户的终端执行编译后的程序，没有得到预期结果，我发现程序在用户目录下没有正确执行文件的创建。

原因是对 ```creat(const char* pathname, mode_t mode)```这个函数接口的使用有误。

creat函数接口相当于```open(pathname, O_CREAT|O_WRONLY|O_TRUNC, mode)```。也就是说，第二个参数 mode 必须传的是“文件权限”（比如 0644、0666 这种八进制数字），而不是 O_RDWR、O_CREAT 这类“打开标志”。

HashFile.c中函数的定义为```int hashfile_creat(const char* filename, mode_t mode, int reclen, int total_rec_num);```然后在hashfile_creat()函数中调用系统函数creat().

程序在jtRecord.c中的调用为```int fd = hashfile_creat(FileNAME, O_RDWR | O_CREAT, RECORDLEN, 6);``` 相当于把打开方式 O_RDWR|O_CREAT (2|64=66) 作为权限位传给了mode.

传给 creat() 后，系统就会用八进制 0102（十进制66 = 八进制0102）作为权限去创建文件。0102 解析下来相当于“拥有者只有执行权限 (0100)，其他用户只有写权限 (0002)”，导致：文件所有者（也就是执行程序的用户）没有写权限，只有“其他人”才有写权限，于是后续想往这个文件里写数据（比如写入文件头、CFTag 等）就会“权限不足”而失败。
