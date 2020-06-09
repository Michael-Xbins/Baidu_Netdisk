#include"ftp.h"

int main(int argc,char *argv[])
{
	check_args(argc,3);
	int sfd,fd;
	struct sockaddr_in ser;
	bzero(&ser,sizeof(ser));
	ser.sin_family=AF_INET;
	ser.sin_addr.s_addr=inet_addr(argv[1]);
	ser.sin_port=htons(atoi(argv[2]));
	sfd=socket(AF_INET,SOCK_STREAM,0);
	connect(sfd,(struct sockaddr*)&ser,sizeof(ser));
	int n=verify(sfd);
	if(-1==n)
		return -1;
	else if(!n)
	{
		char buf[2048]={0};
		bzero(buf,sizeof(buf));
		while(1)
		{
			bzero(buf,sizeof(buf));
			fgets(buf,sizeof(buf),stdin);
			buf[strlen(buf)-1]='\0';
			puts(buf);
			system("clear");

			if(!strcmp(buf,"ls")||!strcmp(buf,"pwd"))
			{
				send(sfd,buf,strlen(buf),0);
				bzero(buf,sizeof(buf));
				recv(sfd,buf,sizeof(buf),0);
				printf("%s\n",buf);
			}

			else if(!strncmp(buf,"cd",2))
			{
				if(buf[2]=='\0'||buf[2]!=' '||buf[3]=='\0'||buf[3]==' ')
				{
					printf("please input the directory name after cd\n");
				}
				else
				{
					send(sfd,buf,strlen(buf),0);
					bzero(buf,sizeof(buf));
					recv(sfd,buf,sizeof(buf),0);
					printf("%s\n",buf);
				}
			}

			else if(!strncmp(buf,"remove",6))
			{
				if(buf[6]=='\0'||buf[6]!=' '||buf[7]=='\0'||buf[7]==' ')
				{
					printf("please input the file name after remove\n");
				}
				else
				{
					puts(buf);
					send(sfd,buf,strlen(buf),0);
					recv(sfd,buf,sizeof(buf),0);
					puts(buf);
				}
			}

			else if(!strncmp(buf,"gets",4))
			{
				if(buf[4]=='\0'||buf[4]!=' '||buf[5]=='\0'||buf[5]==' ')
				{
					printf("please input the file name after gets\n");
				}
				else
				{
					struct stat temp;
					char file_name[512]={0};
					char path[1024]={0};
					strcpy(file_name,buf+5);
					getcwd(path,sizeof(path));
					puts(buf);
					send(sfd,buf,strlen(buf),0);
					bzero(buf,sizeof(buf));
					recv(sfd,buf,sizeof(buf),0);

					if(!strcmp(buf,"directory"))
					{
						printf("Unable to transfer directory\n");
					}
					else if(!strcmp(buf,"error"))
					{
						printf("The file is not exist or error\n");
					}
					else if(!strcmp(buf,"file"))
					{
						bzero(buf,sizeof(buf));
						int len=0;
						off_t file_size=0;
						double download_size=0;
						off_t downloaded_size=0;
						int ret=judge_file(path,file_name);
						if(!ret)
						{
							strcat(path,"/");
							strcat(path,file_name);
							stat(path,&temp);
							downloaded_size=temp.st_size;
							write(sfd,&downloaded_size,sizeof(off_t));//实现断点续传

							//接文件大小
							fd=open(path,O_RDWR,0666);
							check_error(-1,fd,"open");
							recv_n(sfd,(char*)&len,4);
							recv_n(sfd,(char*)&file_size,len);
							lseek(fd,downloaded_size,SEEK_SET);

							file_size=file_size+downloaded_size;
							download_size=download_size+downloaded_size;

							//接文件内容:按秒打印下载的百分比
							time_t start,end;
							int ret;
							start=time(NULL);
							while(1)
							{
								bzero(buf,sizeof(buf));
								ret=recv_n(sfd,(char*)&len,4);
								if(ret!=-1&&len>0)
								{
									ret=recv_n(sfd,buf,len);
									if(ret==-1)
									{
										printf("download %5.2lf%s\n",download_size/file_size*100,"%");
										break;
									}
									write(fd,buf,len);
									download_size=download_size+len;
									end=time(NULL);
									if(end-start>=1)
									{
										printf("download %5.2lf%s\r",download_size/file_size*100,"%");
										fflush(stdout);
										start=end;
									}
								}
								else    //len等于0时，即读到了文件结束标志
								{
									printf("download success 100%s\n","%");
									break;
								}
							}
						}
						else
						{
							write(sfd,&file_size,sizeof(off_t));
							//接文件名
							recv_n(sfd,(char*)&len,4);
							recv_n(sfd,buf,len);

							//接文件大小
							fd=open(buf,O_RDWR|O_CREAT,0666);
							check_error(-1,fd,"open");
							recv_n(sfd,(char*)&len,4);
							recv_n(sfd,(char*)&file_size,len);

							/*if(file_size>104857600)
							  {
							  ret=ftruncate(fd,file_size);
							  if(ret==-1)
							  {
							  perror("ftruncate");
							  return -1;
							  }
							  char *p=(char*)mmap(NULL,file_size,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
							  printf("%p\n",p);
							  ret=recv_n(sfd,p,file_size);
							  printf("recv_n ret=%d\n",ret);
							  munmap(p,file_size);
							  if(!ret)
							  printf("downloaded 100%%\n"); 
							  }*/

							//接文件内容:按秒打印下载的百分比
							time_t start,end;
							int ret;
							start=time(NULL);
							while(1)
							{
								bzero(buf,sizeof(buf));
								ret=recv_n(sfd,(char*)&len,4);
								if(ret!=-1&&len>0)
								{
									ret=recv_n(sfd,buf,len);
									if(ret==-1)
									{
										printf("download %5.2lf%s\n",download_size/file_size*100,"%");
										break;
									}
									write(fd,buf,len);
									download_size=download_size+len;
									end=time(NULL);
									if(end-start>=1)
									{
										printf("download %5.2lf%s\r",download_size/file_size*100,"%");
										fflush(stdout);
										start=end;
									}
								}
								else    //len等于0时，即读到了文件结束标志
								{
									printf("download success 100%s\n","%");
									break;
								}
							}
						}
					}
				}
			}
			else if(!strncmp(buf,"puts",4))
			{
				if(buf[4]=='\0'||buf[4]!=' '||buf[5]=='\0'||buf[5]==' ')
				{
					printf("please input the file name after puts\n");
				}
				else
				{
					char file_name[1024]={0};
					char path[1024]={0};
					getcwd(path,sizeof(path));
					strcpy(file_name,buf+5);
					int ret_value=judge_file(path,file_name);
					if(1==ret_value)
					{
						printf("Unable to transfer directory\n");
					}
					else if(-1==ret_value)
					{
						printf("The file is not exist or error\n");
					}
					else if(!ret_value)
					{
						send(sfd,buf,strlen(buf),0);
						client_tran_file(sfd,path,file_name);
						bzero(buf,sizeof(buf));
						recv(sfd,buf,sizeof(buf),0);
						puts(buf);
					}
				}
			}
			else
				printf("Error command\n");
		}
		close(fd);
		close(sfd);
		return 0;
	}
}
