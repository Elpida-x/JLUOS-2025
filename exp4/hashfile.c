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
