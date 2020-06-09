#ifndef __FACTORY_H__
#define __FACTORY_H__
#include "head.h"
#include "work_que.h"
#define STR_LEN 10 //定义随机输出的字符串长度

//typedef函数指针：要区别于 typedef结构体，其没有别名这一说，其本身就是别名。
typedef void* (*thread_func_t)(void*p);//typedef一个线程函数指针类型thread_func_t

typedef struct					//线程池主要的数据结构
{
	pthread_t *pth_id;			//线程id的起始地址（在申请的空间内保存所有子线程的id）
	int pthread_num;			//创建的线程数目
	que_t que;					//队列
	//线程函数（函数指针）: 当有多个业务线程时，可把这些 线程函数的 函数指针存到此处。
	thread_func_t threadfunc;
	pthread_cond_t cond;
	short start_flag;			//线程是否启动标志
}factory,*pfac;

int factory_init(pfac,thread_func_t,int,int);		//以下为函数声明
int factory_start(pfac);
int tcp_start_listen(int*,char*,char*,int);			//封装了socket、bind、listen
int send_n(int,char*,int);
int recv_n(int,char*,int);
int serve_tran_file(pnode_t);
void printls(char *);
int judge_file(char *,char *);
void get_salt(char*,char*); 
void GenerateSalt(int,char *);
int query_name(char *,int,pnode_t);
int login_mysql(int sfd,char *,pnode_t);

typedef struct
{
	int len;
	char buf[1000];
}train;

#endif
