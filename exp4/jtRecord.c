// 测试文件
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
