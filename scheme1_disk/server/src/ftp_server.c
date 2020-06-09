#include "../include/factory.h"

int exit_fds[2],puts_fds[2];
void sig_exit_func(int signum)
{
	printf("signum is %d\n",signum);
	char flag=1;
	write(exit_fds[1],&flag,1); //任何一个线程都可能来响应信号
}

void* download_file(void* p)
{
	pfac pf=(pfac)p;
	pque_t pq=&pf->que;
	pnode_t pcur;
	while(1)
	{
		pthread_mutex_lock(&pq->que_mutex);
		if(0==pq->size)
		{
			pthread_cond_wait(&pf->cond,&pq->que_mutex);
		}
		judge_thread_exit(pq);//切记要放到que_get前面
		que_get(pq,&pcur);
		pthread_mutex_unlock(&pq->que_mutex);

		if(pcur!=NULL&&!strncmp(pcur->opt,"gets",4))
		{
			serve_tran_file(pcur);
			free(pcur);
		}
		else if(pcur!=NULL&&!strncmp(pcur->opt,"puts",4))
		{
			recv_file(pcur->new_fd,pcur->path,pcur->opt);
			free(pcur);
			char c='h';
			write(puts_fds[1],&c,1);
		}
	}
}

int main(int argc,char* argv[])
{
	pipe(exit_fds);
	pipe(puts_fds);
	if(fork()) //信号发送给父进程，由父进程通知子进程中的线程池退出
	{
		close(exit_fds[0]);
		signal(SIGUSR1,sig_exit_func);
		pid_t pid=wait(NULL);
		printf("child process pid=%d\n",pid);
		return 0;	//父进程在此结束
	}
	close(exit_fds[1]);
	if(argc!=5)
	{
		printf("./server IP PORT THREAD_NUM CAPACITY");
		return -1;
	}
	factory f;
	int thread_num=atoi(argv[3]);
	int capacity=atoi(argv[4]);
	factory_init(&f,download_file,thread_num,capacity);
	factory_start(&f);//开启线程池

	int sfd;
	tcp_start_listen(&sfd,argv[1],argv[2],capacity);
	int new_fd;
	pque_t pq=&f.que;
	pnode_t pcur;

	int ret;
	int epfd=epoll_create(1);
	struct epoll_event event,evs[3];
	event.events=EPOLLIN;
	event.data.fd=sfd;
	ret=epoll_ctl(epfd,EPOLL_CTL_ADD,sfd,&event);
	check_error(-1,ret,"epoll_ctl");
	event.data.fd=exit_fds[0];
	ret=epoll_ctl(epfd,EPOLL_CTL_ADD,exit_fds[0],&event);
	check_error(-1,ret,"epoll_ctl");

	int client_num=0;
	char buf[2048]={0};
	bzero(buf,sizeof(buf));
	node_t task[N];  //实现长短命令分离:用1个结构体数组(记录每个客户端信息)、epoll实现
	bzero(task,sizeof(node_t));
	for(int i=0;i<N;i++)
	{
		task[i].new_fd=-1;
	}

	while(1)
	{
		ret=epoll_wait(epfd,evs,3,-1);
		for(int i=0;i<ret;i++)
		{
			if(evs[i].data.fd==sfd)
			{
				int j=0;
				char temp[512]={0};
				char salt[512]={0};
				struct spwd *sp;
				new_fd=accept(sfd,NULL,NULL);
				recv(new_fd,temp,sizeof(temp),0);
				if((sp=getspnam(temp))==NULL)
				{
					int n=-1;
					send(new_fd,&n,sizeof(n),0);
					perror("获取用户名和密码");
				}
				else
				{
					int n=0;
					send(new_fd,&n,sizeof(n),0);
					
					get_salt(salt,sp->sp_pwdp);
					send(new_fd,salt,strlen(salt),0);
					bzero(temp,sizeof(temp));
					recv(new_fd,temp,sizeof(temp),0);   //收到盐值和密码一起加密的密文
					if(strcmp(sp->sp_pwdp,temp))
					{
						int n=-1;
						send(new_fd,&n,sizeof(n),0);
					}
					else
					{
						int n=0;
						send(new_fd,&n,sizeof(n),0);
						while(j!=N)
						{
							if(-1==task[j].new_fd)
								break;
							j++;
							if(j==N)
							{
								j=0;		//若超过了N个客户连接，先令其一直等待
							}
						}
						task[j].new_fd=new_fd;
						getcwd(task[j].path,sizeof(task[j].path));
						event.data.fd=new_fd;
						ret=epoll_ctl(epfd,EPOLL_CTL_ADD,task[j].new_fd,&event);
						check_error(-1,ret,"epoll_ctl");
						client_num+=1;
						printf("%d connect\n",client_num);
					}
				}
			}

			for(int j=0;j<N;j++)
			{
				if(evs[i].data.fd==task[j].new_fd)
				{
					bzero(buf,sizeof(buf));
					recv(task[j].new_fd,buf,sizeof(buf),0);

					if(!strcmp(buf,"pwd"))
					{
						bzero(buf,sizeof(buf));
						strcpy(buf,task[j].path);
						send(task[j].new_fd,buf,strlen(buf),0);
					}

					else if(!strcmp(buf,"ls"))
					{
						bzero(buf,sizeof(buf));
						strcpy(buf,task[j].path);
						printls(buf);
						send(task[j].new_fd,buf,strlen(buf)-1,0);
					}

					else if(!strncmp(buf,"cd ",3))
					{
						bzero(task[j].file_name,sizeof(task[j].file_name));
						strcpy(task[j].file_name,buf+3);
						int ret_value=judge_file(task[j].path,task[j].file_name);
						if(ret_value!=1)
						{
							bzero(buf,sizeof(buf));
							strcpy(buf,"Not a directory");
							send(task[j].new_fd,buf,strlen(buf),0);
						}
						else
						{
							bzero(buf,sizeof(buf));
							strcpy(buf,task[j].path);

							if(!strcmp(task[j].file_name,"."))
							{
								send(task[j].new_fd,task[j].path,strlen(task[j].path),0);
							}

							else if(!strcmp(task[j].file_name,".."))
							{
								int i=0;
								if(!strcmp(buf,"/"))
								{
									bzero(buf,sizeof(buf));
									strcpy(buf,"error: can't go back");
									send(task[j].new_fd,buf,strlen(buf),0);
								}
								else
								{
									for(i=strlen(buf);i>=0;i--)
									{
										if(0==i&&buf[i]=='/')
										{
											bzero(buf+i+1,strlen(buf+i+1));
											bzero(task[j].path,sizeof(task[j].path));
											strcpy(task[j].path,buf);
										}
										if(0!=i&&buf[i]=='/')
										{
											bzero(buf+i,strlen(buf+i));
											bzero(task[j].path,sizeof(task[j].path));
											strcpy(task[j].path,buf);
											break;
										}
									}
									send(task[j].new_fd,task[j].path,strlen(task[j].path),0);
								}
							}

							else if(task[j].file_name[0]=='/')  //cd绝对路径
							{
								bzero(task[j].path,sizeof(task[j].path));
								strcpy(task[j].path,task[j].file_name);
								send(task[j].new_fd,task[j].path,strlen(task[j].path),0);
							}

							else   //cd目录名
							{
								if(!strcmp(buf,"/"))
								{
									strcat(buf,task[j].file_name);
									bzero(task[j].path,sizeof(task[j].path));
									strcpy(task[j].path,buf);
								}
								else
								{
									strcat(buf,"/");
									strcat(buf,task[j].file_name);
									bzero(task[j].path,sizeof(task[j].path));
									strcpy(task[j].path,buf);
								}
								send(task[j].new_fd,task[j].path,strlen(task[j].path),0);
							}
						}
					}

					else if(!strncmp(buf,"remove ",7))
					{
						bzero(task[j].file_name,sizeof(task[j].file_name));
						strcpy(task[j].file_name,buf+7);
						bzero(buf,sizeof(buf));
						strcpy(buf,task[j].path);
						strcat(buf,"/");
						strcat(buf,task[j].file_name);
						int ret_value=judge_file(task[j].path,task[j].file_name);
						if(-1==ret_value)
						{
							bzero(buf,sizeof(buf));
							strcpy(buf,"The file is not exist");
							send(task[j].new_fd,buf,strlen(buf),0);
						}
						if(!ret_value||1==ret_value)
						{
							remove(buf);
							bzero(buf,sizeof(buf));
							strcpy(buf,"already deleted");
							send(task[j].new_fd,buf,strlen(buf),0);
						}
					}

					else if(!strncmp(buf,"gets ",5))
					{
						bzero(task[j].file_name,sizeof(task[j].file_name));
						bzero(task[j].opt,sizeof(task[j].opt));
						strcpy(task[j].file_name,buf+5);
						strcpy(task[j].opt,buf);
						int ret_value=judge_file(task[j].path,task[j].file_name);
						if(ret_value==1)
						{
							bzero(buf,sizeof(buf));
							strcpy(buf,"directory");
							send(task[j].new_fd,buf,strlen(buf),0);
						}
						else if(-1==ret_value)
						{
							bzero(buf,sizeof(buf));
							strcpy(buf,"error");
							send(task[j].new_fd,buf,strlen(buf),0);
						}
						else if(!ret_value)
						{
							bzero(buf,sizeof(buf));
							strcpy(buf,"file");
							send(task[j].new_fd,buf,strlen(buf),0);
							read(task[j].new_fd,&task[j].breakpoint,sizeof(off_t));

							pcur=(pnode_t)calloc(1,sizeof(node_t));
							pcur->new_fd=task[j].new_fd;
							pcur->breakpoint=task[j].breakpoint;
							strcpy(pcur->path,task[j].path);
							strcpy(pcur->file_name,task[j].file_name);
							strcpy(pcur->opt,task[j].opt);
							pthread_mutex_lock(&pq->que_mutex);
							que_insert(pq,pcur);
							pthread_mutex_unlock(&pq->que_mutex);
							pthread_cond_signal(&f.cond);
//用于线程池刚开启还没有接收任务时就要退出，此时队列中还没有放入一个结点，子线程全在睡觉,则此种情形要群体唤醒子线程， 各自去取 退出结点 并 线程退出。
						}
					}
					else if(!strncmp(buf,"puts ",5))
					{
						bzero(task[j].opt,sizeof(task[j].opt));
						strcpy(task[j].file_name,buf+5);
						strcpy(task[j].opt,buf);

						pcur=(pnode_t)calloc(1,sizeof(node_t));
						pcur->new_fd=task[j].new_fd;
						strcpy(pcur->path,task[j].path);
						strcpy(pcur->file_name,task[j].file_name);
						strcpy(pcur->opt,task[j].opt);

						pthread_mutex_lock(&pq->que_mutex);
						que_insert(pq,pcur);
						pthread_mutex_unlock(&pq->que_mutex);
						pthread_cond_signal(&f.cond);//用于线程池刚开启还没有接收任务时就要退出，>
						//此时主线程不再接收此客户端信息，要由子线程去接收(卡了很长时间处理此问题，一定要注意！！！)
						char c;
						read(puts_fds[0],&c,1);
						printf("c=%c\n",c);
					}
				}
			}

			if(evs[i].data.fd==exit_fds[0])
			{
				close(sfd);
				pcur=(pnode_t)calloc(1,sizeof(node_t));
				pcur->new_fd=-1;  //退出机制
				pthread_mutex_lock(&pq->que_mutex);
				que_insert_exit(pq,pcur);
				pthread_mutex_unlock(&pq->que_mutex);
				pthread_cond_broadcast(&f.cond);//防止在放入退出结点前，队列中一个结点都没有了，子线程此时全在睡觉,则此种情形要群体唤醒。
				for(int j=0;j<thread_num;j++)
				{
					pthread_join(f.pth_id[j],NULL);
				}
				exit(0); //让子进程退出
			}
		}
	}
}
