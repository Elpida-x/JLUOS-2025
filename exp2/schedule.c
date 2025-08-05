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
