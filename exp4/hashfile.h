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
