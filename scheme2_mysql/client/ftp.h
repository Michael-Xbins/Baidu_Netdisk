#include <sys/uio.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <pthread.h>
#include <signal.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <syslog.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <grp.h>
#include <pwd.h>
#include <fcntl.h>
#include<shadow.h>
#include<errno.h>
#include <crypt.h>

#define check_args(a,b) {if(a!=b) {printf("error args\n");\
	return -1;}}
#define check_error(ret_val,ret,func_name) {if(ret_val==ret){\
	perror(func_name);return -1;}}
#define check_thread_error(ret,func_name) {if(0!=ret){\
	printf("%s is failed %d\n",func_name,ret);return -1;}}
typedef struct{
	pid_t pid;
	int fds;//子进程的管道对端
	short busy_flag;//忙碌标志，0代表非忙碌，1代码忙碌
}Data_t,*pData_t;

int make_child(pData_t,int);
void child_handle(int);
int recv_fd(int,int*);
int send_fd(int,int);
int send_n(int,char*,int);
int recv_n(int,char*,int);
int judge_file(char *,char *);
int client_tran_file(int,char *,char *);
int verify(int);
int client_login(int);

typedef struct{
	int len;
	char buf[1000];
}train;

