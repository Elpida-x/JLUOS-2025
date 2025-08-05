#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/file.h>
#include <unistd.h>

// 全局变量定义
char r_buf[4];         // 读缓冲区
char w_buf[4];         // 写缓冲区
int pipe_fd[2];        // 管道文件描述符 (0:读端, 1:写端)
pid_t pid1, pid2, pid3, pid4;  // 进程ID

// 函数声明
int producer(int id);
int consumer(int id);

int main(int argc, char **argv) 
{  
    // 创建管道
    if (pipe(pipe_fd) < 0) {
        printf("pipe create error\n");
        exit(-1);
    } else {
        printf("pipe %d is created successfully!\n", getpid());
        
        // 创建2个生产者进程
        if ((pid1 = fork()) == 0)
            producer(1);
        if ((pid2 = fork()) == 0)
            producer(2);
        
        // 创建2个消费者进程
        if ((pid3 = fork()) == 0)
            consumer(1);
        if ((pid4 = fork()) == 0)
            consumer(2);
    }

    // 关闭父进程的管道描述符
    close(pipe_fd[0]);
    close(pipe_fd[1]);
    
    // 等待所有子进程结束
    int pid, status;
    for (int i = 0; i < 4; i++) {
        pid = wait(&status);
    }
      
    exit(0);
}

/**
 * 生产者进程函数
 * @param id 生产者ID
 * @return 进程退出状态
 */
int producer(int id)
{
    printf("producer %d is running!\n", id);
    close(pipe_fd[0]);  // 关闭读端，只保留写端

    // 生产9个产品
    for (int i = 1; i < 10; i++) {
        sleep(3);  // 模拟生产耗时
        
        // 根据生产者ID生成不同产品
        if (id == 1)
            strcpy(w_buf, "aaa");  // 自动添加字符串结束符
        else
            strcpy(w_buf, "bbb");
        
        // 写入管道
        if (write(pipe_fd[1], w_buf, 4) == -1)
            printf("write to pipe error\n");
    }

    close(pipe_fd[1]);  // 写完后关闭写端
    printf("producer %d is over!\n", id);
    exit(id);  // 退出进程并返回状态
}

/**
 * 消费者进程函数
 * @param id 消费者ID
 * @return 进程退出状态
 */
int consumer(int id)
{
    close(pipe_fd[1]);  // 关闭写端，只保留读端
    printf("consumer %d is running!\n", id);

    // 验证进程空间独立性：修改w_buf不影响其他进程
    if (id == 1)
        strcpy(w_buf, "ccc");
    else
        strcpy(w_buf, "ddd");

    // 持续读取管道内容
    while (1) {
        sleep(1);  // 模拟消费耗时
        strcpy(r_buf, "eee");  // 初始化读缓冲区
        
        // 读取管道，返回0表示已读完所有数据
        if (read(pipe_fd[0], r_buf, 4) == 0)
            break;
        
        printf("consumer %d get %s, while the w_buf is %s\n", 
               id, r_buf, w_buf);
    }

    close(pipe_fd[0]);  // 读完后关闭读端
    printf("consumer %d is over!\n", id);
    exit(id);  // 退出进程并返回状态
}
