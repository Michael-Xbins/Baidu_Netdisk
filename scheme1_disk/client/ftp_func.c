#include"ftp.h"

void sigfunc(int signum)
{
	printf("%d is coming\n",signum);
}
int client_tran_file(int sfd,char *path,char *file_name)
{
	train t;
	signal(SIGPIPE,sigfunc);
	char temp[2048]={0};
	strcpy(temp,path);
	strcat(temp,"/");
	strcat(temp,file_name);

	//先发文件名
	t.len=strlen(file_name);
	strcpy(t.buf,file_name);
	int ret,fd;
	ret=send_n(sfd,(char*)&t,4+t.len);
	if(-1==ret)			    //若客户端在下载的过程中突然断开了，则服务器端 tran_file.c的 send第一次返回-1时就要做判断，下同。
	{
		close(sfd);
		return 0;
	}

	//发文件大小
	fd=open(temp,O_RDONLY);
	check_error(-1,fd,"open");
	struct stat buf;
	fstat(fd,&buf);
	t.len=sizeof(buf.st_size);   // t.len中存的是8个字节（off_t是8个字节）注意：此处要传的是8，而不是长整型数: 因为文件大小 即是要发送的 文件内容。
	memcpy(t.buf,&buf.st_size,sizeof(off_t));//传长整型数(文件大小)到buf中
	ret=send_n(sfd,(char*)&t,4+t.len);
	if(-1==ret)
	{
		close(sfd);
		return 0;
	}

	//发文件内容
	while((t.len=read(fd,t.buf,sizeof(t.buf)))>0)
		//注意： t.len一定不能用strlen来算
	{
		ret=send_n(sfd,(char*)&t,4+t.len); 
		if(-1==ret)
		{
			close(sfd);
			return 0;
		}
	}

	//发送结束标志
	ret=send_n(sfd,(char*)&t,4+t.len);	 //此时刚好t.len中存的就是0
	if(-1==ret)
	{
		close(sfd);
		return 0;
	}
	return 0;
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


int verify(int sfd)
{
	int n;
	char *passwd;
	char salt[512]={0};
	char buf[512]={0};
	printf("请输入用户名：\n");
	fgets(buf,sizeof(buf),stdin);
	buf[strlen(buf)-1]='\0';
	send(sfd,buf,strlen(buf),0);
	bzero(buf,sizeof(buf));
	recv(sfd,&n,sizeof(n),0);
	if(-1==n)
	{
		printf("The user does not exist or Permission denied\n");
		return -1;
	}
	else if(!n)
	{
		//输入用户名密码(不会回写)
		passwd=getpass("请输入密码：");
		recv(sfd,salt,sizeof(salt),0);//服务器发来的盐值salt
		bzero(buf,sizeof(buf));
		const char *p=crypt(passwd,salt);
		strcpy(buf,p);
		send(sfd,buf,strlen(buf),0);
		int n;
		recv(sfd,&n,sizeof(n),0);
		if(-1==n)
		{
			printf("验证失败\n");
			return -1;
		}
		else if(0==n)
		{
			printf("验证通过\n");
			return 0;
		}
	}
	return -1;
}  
