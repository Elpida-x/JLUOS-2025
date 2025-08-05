#include <math.h>
#include <sched.h>
#include <pthread.h>
#include <stdlib.h>
#include <semaphore.h> 
#include <stdio.h>
#include <unistd.h>

// 实时任务结构体定义
typedef struct {
    char task_id;       // 任务标识符
    int call_num;       // 任务调用次数
    int ci;             // 任务计算时间
    int ti;             // 任务周期时间
    int ci_left;        // 剩余计算时间
    int ti_left;        // 剩余周期时间
    int flag;           // 任务活跃状态（0:不活跃, 2:活跃）
    int arg;            // 任务序号
    pthread_t th;       // 任务对应的线程
} task;

// 函数声明
void proc(int* args);
void* idle();
int select_proc(int alg);

// 全局变量定义
int task_num = 0;                 // 实时任务数量
int idle_num = 0;                 // 闲逛任务执行次数
int alg;                          // 所选算法（1:EDF, 2:RMS）
int curr_proc = -1;               // 当前选中的任务索引
int demo_time = 100;              // 演示时间长度（时间单位）
task* tasks;                      // 动态分配的任务数组
pthread_mutex_t proc_wait[100];   // 任务线程互斥锁
pthread_mutex_t main_wait;        // 主线程管理锁
pthread_mutex_t idle_wait;        // 闲逛线程锁
float sum = 0;                    // 用于可调度性判断
pthread_t idle_proc;              // 闲逛进程线程

int main(int argc, char**argv) {
    // 初始化互斥锁
    pthread_mutex_init(&main_wait, NULL);
    pthread_mutex_lock(&main_wait);  // 主线程自我锁定，等待唤醒
    pthread_mutex_init(&idle_wait, NULL);
    pthread_mutex_lock(&idle_wait);  // 闲逛线程初始等待

    // 输入实时任务数量
    printf("Please input number of real time tasks:\n");
    scanf("%d", &task_num);
    tasks = (task*)malloc(task_num * sizeof(task));

    // 初始化任务互斥锁
    int i;
    for (i = 0; i < task_num; i++) {
        pthread_mutex_init(&proc_wait[i], NULL);
        pthread_mutex_lock(&proc_wait[i]);  // 任务初始等待调度
    }

    // 输入各任务参数
    for (i = 0; i < task_num; i++) {
        printf("Please input task id, followed by Ci and Ti:\n");
        getchar();  // 吸收换行符
        scanf("%c %d %d", &tasks[i].task_id, &tasks[i].ci, &tasks[i].ti);
        tasks[i].ci_left = tasks[i].ci;
        tasks[i].ti_left = tasks[i].ti;
        tasks[i].flag = 2;  // 标记为活跃
        tasks[i].arg = i;
        tasks[i].call_num = 1;
        sum += (float)tasks[i].ci / (float)tasks[i].ti;  // 计算利用率
    }

    // 选择调度算法
    printf("Please input algorithm, 1 for EDF, 2 for RMS:");
    scanf("%d", &alg);
    printf("Please input demo time:");
    scanf("%d", &demo_time);

    // 可调度性判断
    double r = 1;  // EDF算法默认可调度上限
    if (alg == 2) {  // RMS算法
        r = (double)task_num * (exp(log(2) / (double)task_num) - 1);
        printf("r is %lf\n", r);
    }

    // 检查是否可调度
    if (sum > r) {
        printf("(sum=%lf > r=%lf) ,not schedulable!\n", sum, r);
        exit(2);
    }

    // 创建闲逛线程和实时任务线程
    pthread_create(&idle_proc, NULL, (void*)idle, NULL);
    for (i = 0; i < task_num; i++) {
        pthread_create(&tasks[i].th, NULL, (void*)proc, &tasks[i].arg);
    }

    // 模拟时间调度（时钟中断）
    for (i = 0; i < demo_time; i++) {
        int j;
        // 选择下一个要执行的任务
        if ((curr_proc = select_proc(alg)) != -1) {
            // 唤醒选中的任务线程
            pthread_mutex_unlock(&proc_wait[curr_proc]);
            pthread_mutex_lock(&main_wait);  // 等待任务执行完成
        } else {
            // 无可执行任务，运行闲逛线程
            pthread_mutex_unlock(&idle_wait);
            pthread_mutex_lock(&main_wait);
        }

        // 更新所有任务的周期状态
        for (j = 0; j < task_num; j++) {
            if ((--tasks[j].ti_left) == 0) {  // 周期结束
                tasks[j].ti_left = tasks[j].ti;  // 重置周期
                tasks[j].ci_left = tasks[j].ci;  // 重置计算时间
                pthread_create(&tasks[j].th, NULL, (void*)proc, &tasks[j].arg);
                tasks[j].flag = 2;  // 标记为活跃
            }
        }
    }

    printf("\n");
    sleep(10);
    exit(0);
}

/**
 * 任务执行函数
 * @param args 任务索引
 */
void proc(int* args) {
    // 执行任务直到本次计算时间用完
    while (tasks[*args].ci_left > 0) {
        pthread_mutex_lock(&proc_wait[*args]);  // 等待被调度

        // 输出闲逛任务执行次数（如果有）
        if (idle_num != 0) {
            printf("idle(%d)", idle_num);
            idle_num = 0;
        }

        // 输出当前执行的任务
        printf("%c%d", tasks[*args].task_id, tasks[*args].call_num);
        tasks[*args].ci_left--;  // 减少剩余计算时间

        // 任务执行完成
        if (tasks[*args].ci_left == 0) {
            printf("(%d)", tasks[*args].ci);  // 输出总计算时间
            tasks[*args].flag = 0;  // 标记为不活跃
            tasks[*args].call_num++;  // 增加调用次数
        }

        pthread_mutex_unlock(&main_wait);  // 唤醒主线程
    }
}

/**
 * 闲逛线程函数
 */
void* idle() {
    while (1) {
        pthread_mutex_lock(&idle_wait);  // 等待被调度
        printf("->");  // 表示一个时间单位的空闲
        idle_num++;
        pthread_mutex_unlock(&main_wait);  // 唤醒主线程
    }
}

/**
 * 任务选择函数（调度算法核心）
 * @param alg 调度算法（1:EDF, 2:RMS）
 * @return 选中的任务索引
 */
int select_proc(int alg) {
    int j;
    int temp1 = 10000;  // 用于比较的临时变量
    int temp2 = -1;     // 选中的任务索引

    // RMS算法：如果当前任务活跃则继续执行（优先级抢占）
    if ((alg == 2) && (curr_proc != -1) && (tasks[curr_proc].flag != 0)) {
        return curr_proc;
    }

    // 遍历所有任务选择下一个执行的任务
    for (j = 0; j < task_num; j++) {
        if (tasks[j].flag == 2) {  // 只考虑活跃任务
            switch (alg) {
                case 1:  // EDF算法：最短剩余周期优先
                    if (temp1 > tasks[j].ti_left) {
                        temp1 = tasks[j].ti_left;
                        temp2 = j;
                    }
                    break;
                case 2:  // RMS算法：最短周期优先
                    if (temp1 > tasks[j].ti) {
                        temp1 = tasks[j].ti;
                        temp2 = j;
                    }
                    break;
            }
        }
    }
    return temp2;
}
