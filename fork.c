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
