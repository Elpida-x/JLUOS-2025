#define RECORDLEN 32 //记录的长度固定为32字节
struct jtRecord
{
	int key;							 // 4字节键值
	char other[RECORDLEN - sizeof(int)]; // 28字节其他数据
};
#ifdef HAVE_CONFIG_H //条件编译
#include<config.h>
#endif
