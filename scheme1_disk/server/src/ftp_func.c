#include "../include/factory.h"


void sigfunc(int signum)
{
	printf("%d is coming\n",signum);
}

int serve_tran_file(pnode_t pcur)
{
	int ret,fd;
	train t;
	signal(SIGPIPE,sigfunc);
	char temp[2048]={0};
	strcpy(temp,pcur->path);
	strcat(temp,"/");
	strcat(temp,pcur->file_name);
	if(pcur->breakpoint)
	{
		//发文件大小
		fd=open(temp,O_RDONLY);
		check_error(-1,fd,"open");
		struct stat buf;
		fstat(fd,&buf);
		t.len=sizeof(buf.st_size);   // t.len中存的是8个字节（off_t是8个字节）注意：此处要传的是8，而不是>
		buf.st_size=buf.st_size-(pcur->breakpoint);
		memcpy(t.buf,&buf.st_size,sizeof(off_t));//传长整型数(文件大小)到buf中
		ret=send_n(pcur->new_fd,(char*)&t,4+t.len);
		if(-1==ret)
		{
			close(pcur->new_fd);
			return 0;
		}

		/*if(buf.st_size>104857600)
		  {
		  p=(char*)mmap(NULL,buf.st_size,PROT_READ|PROT_WRITE,MAP_SHARED,fd,pcur->breakpoint);
		  lseek(fd,pcur->breakpoint,SEEK_SET);
		  ret=send_n(pcur->new_fd,p,buf.st_size);  //发文件内容
		  if(-1==ret)
		  {
		  close(pcur->new_fd);
		  return 0;
		  }
		  }*/

		//发文件内容
		lseek(fd,pcur->breakpoint,SEEK_SET);
		while((t.len=read(fd,t.buf,sizeof(t.buf)))>0)
			//注意： t.len一定不能用strlen来算
		{
			ret=send_n(pcur->new_fd,(char*)&t,4+t.len); 
			if(-1==ret)
			{
				close(pcur->new_fd);
				return 0;
			}
		}

		//发送结束标志
		ret=send_n(pcur->new_fd,(char*)&t,4+t.len);    //此时刚好t.len中存的就是0
		if(-1==ret)
		{
			close(pcur->new_fd);
			return 0;
		}
	}
	else
	{
		//先发文件名
		t.len=strlen(pcur->file_name);
		strcpy(t.buf,pcur->file_name);
		ret=send_n(pcur->new_fd,(char*)&t,4+t.len);
		if(-1==ret)             //若客户端在下载的过程中突然断开了，则服务器端 tran_file.c的 send第一次返>
		{
			close(pcur->new_fd);
			return 0;
		}

		//发文件大小
		fd=open(temp,O_RDONLY);
		check_error(-1,fd,"open");
		struct stat buf;
		fstat(fd,&buf);
		t.len=sizeof(buf.st_size);   // t.len中存的是8个字节（off_t是8个字节）注意：此处要传的是8，而不是>
		memcpy(t.buf,&buf.st_size,sizeof(off_t));//传长整型数(文件大小)到buf中
		ret=send_n(pcur->new_fd,(char*)&t,4+t.len);
		if(-1==ret)
		{
			close(pcur->new_fd);
			return 0;
		}

		/*if(buf.st_size>104857600)
		  {   
		  p=(char*)mmap(NULL,buf.st_size,PROT_READ|PROT_WRITE,MAP_SHARED,fd,pcur->breakpoint);
		  lseek(fd,pcur->breakpoint,SEEK_SET);
		  ret=send_n(pcur->new_fd,p,buf.st_size);  //发文件内容
		  if(-1==ret)
		  {   
		  close(pcur->new_fd);
		  return 0;
		  }   
		  munmap(p,buf.st_size);
		  }*/  

		//发文件内容
		while((t.len=read(fd,t.buf,sizeof(t.buf)))>0)
			//注意： t.len一定不能用strlen来算
		{
			ret=send_n(pcur->new_fd,(char*)&t,4+t.len); 
			if(-1==ret)
			{
				close(pcur->new_fd);
				return 0;
			}
		}

		//发送结束标志
		ret=send_n(pcur->new_fd,(char*)&t,4+t.len);    //此时刚好t.len中存的就是0
		if(-1==ret)
		{
			close(pcur->new_fd);
			return 0;
		}
	}
	return 0;
}


int send_n(int sfd,char* p,int len)
{
	int total=0;
	int ret;
	while(total<len)
	{   
		ret=send(sfd,p+total,len-total,0);
		//若客户端在下载过程中断开，send第一次会返回-1时，就要做个判断。
		if(-1==ret)
		{   
			printf("client is close\n");
			return -1; 
		}   
		total=total+ret;
	}   
	return 0;
}
int recv_n(int sfd,char* p,int len)
{
	int total=0;
	int ret;
	while(total<len)
	{
		ret=recv(sfd,p+total,len-total,0);//服务器端断开：recv返回值为0
		if(0==ret)  //代表服务器端意外断开（客户端此时要退出）。 注意区别服务器的意外断开（如ctrl+c） 
		{ 
			return -1;
		}
		total=total+ret;
	}
	return 0;
}


int factory_init(pfac pf,thread_func_t threadfunc,int thread_num,int capacity)
{
	bzero(pf,sizeof(factory));
	pf->pth_id=(pthread_t*)calloc(thread_num,sizeof(pthread_t));

	pf->pthread_num=thread_num;
	pf->que.que_capacity=capacity;
	pthread_mutex_init(&pf->que.que_mutex,NULL);
	pf->threadfunc=threadfunc;
	pthread_cond_init(&pf->cond,NULL);
	return 0;
}

int factory_start(pfac pf)
{
	if(!pf->start_flag)
	{
		int i;
		for(i=0;i<pf->pthread_num;i++)
		{
			pthread_create(pf->pth_id+i,NULL,pf->threadfunc,pf);
		}
		pf->start_flag=1;
	}
	return 0;
}

int tcp_start_listen(int *psfd,char *ip,char *port,int backlog)
{
	int sfd=socket(AF_INET,SOCK_STREAM,0);
	int reuse=1;
	int ret;
	ret=setsockopt(sfd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(int));
	check_error(-1,ret,"setsockopt");
	struct sockaddr_in ser;
	bzero(&ser,sizeof(ser));
	ser.sin_family=AF_INET;
	ser.sin_port=htons(atoi(port));
	ser.sin_addr.s_addr=inet_addr(ip);
	ret=bind(sfd,(struct sockaddr*)&ser,sizeof(ser));
	check_error(-1,sfd,"bind");
	listen(sfd,backlog);
	*psfd=sfd;
	return 0;
}

void printls(char *path)
{
	char buf[1024]={0},buf_temp[1024]={0};
	strcpy(buf,path);
	bzero(path,strlen(path));
	struct dirent *p;
	DIR *dir;
	struct stat statbuf;
	dir=opendir(buf);
	int ret;
	if(!dir)
	{
		perror("opendir");
		return;
	}
	while((p=readdir(dir))!=NULL)
	{
		sprintf(buf_temp,"%s%s%s",buf,"/",p->d_name);
		ret=stat(buf_temp,&statbuf);
		if(-1==ret)
		{
			perror("stat");
			return;
		}
		if(p->d_type==4)
			sprintf(path+strlen(path),"%s%s%-25s%s%ld%s","d","  ",p->d_name,"   ",statbuf.st_size,"\n");
		else
			sprintf(path+strlen(path),"%s%s%-25s%s%ld%s","-","  ",p->d_name,"   ",statbuf.st_size,"\n");
	}
	closedir(dir);
}

int judge_file(char *path,char *file_name)
{
	struct dirent *p;
	DIR *dir;

	if(file_name[0]=='/'&&file_name[1]!='/')
	{
		dir=opendir(file_name);
		if(!dir)
		{
			perror("opendir");
			return -1;
		}
		return 1;
	}

	else
	{
		dir=opendir(path);
		if(!dir)
		{
			perror("opendir");
			return -1;
		}
		while((p=readdir(dir))!=NULL)
		{
			if(!strcmp(file_name,p->d_name)&&p->d_type==4)
			{
				return 1;
			}
			if(!strcmp(file_name,p->d_name))
			{
				return 0;
			}
		}
	}
	closedir(dir);
	return -1;
}


void error_quit(const char *msg)
{
	perror(msg);
	exit(-2);
}
void get_salt(char *salt,char *passwd)
{
	int i,j;
	//取出salt、i，记录密码字符下标j，记录$出现次数
	for(i=0,j=0;passwd[i] && j!=3;++i)
	{   
		if(passwd[i]=='$')
			++j;
	}   
	strncpy(salt,passwd,i-1);//将salt格式化，用于crypt加密
}

