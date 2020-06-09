#include "../include/work_que.h"

void que_insert(pque_t pq,pnode_t pnew)
{   
	if(!pq->que_head)
	{
		pq->que_head=pnew;
		pq->que_tail=pnew;
	}else{   
		pq->que_tail->pNext=pnew;
		pq->que_tail=pnew;
	}
	pq->size++;
}

void que_get(pque_t pq,pnode_t* pcur)
{
	*pcur=pq->que_head;
	if(NULL==*pcur)
	{
		return;
	}
	pq->que_head=pq->que_head->pNext;
	if(!pq->que_head)
	{
		pq->que_tail=NULL;
	}
	pq->size--;
}

void que_insert_exit(pque_t pq,pnode_t pnew)
{
	if(NULL==pq->que_head)
	{
		pq->que_head=pnew;
		pq->que_tail=pnew;
	}
	else
	{
		pnew->pNext=pq->que_head;
		pq->que_head=pnew;
	}
	pq->size++;
}

void judge_thread_exit(pque_t pq)
{
	if(-1==pq->que_head->new_fd)
	{
		pthread_mutex_unlock(&pq->que_mutex);
		pthread_exit(NULL);		//此子线程退出了
	}
}

int recv_file(int new_fd,char *path,char *opt)
{
	char path_temp[1024]={0};
	char buf[1024]={0};
	char temp[1024]={0};
	strcpy(temp,opt);
	int len,fd;
	strcpy(path_temp,path);
	strcat(path_temp,"/");
	//接文件名
	recv_n(new_fd,(char*)&len,4);
	recv_n(new_fd,buf,len);
	strcat(path_temp,buf);
	printf("filename=%s,path_temp=%s\n",buf,path_temp);

	//接文件大小
	fd=open(path_temp,O_RDWR|O_CREAT,0666);
	check_error(-1,fd,"open");
	off_t file_size;
	double download_size=0;
	recv_n(new_fd,(char*)&len,4);
	recv_n(new_fd,(char*)&file_size,len);

	//接文件内容:按秒打印下载的百分比
	time_t start,end;
	int ret;
	start=time(NULL);
	while(1)
	{
		bzero(buf,sizeof(buf));
		ret=recv_n(new_fd,(char*)&len,4);
		if(ret!=-1&&len>0)
		{
			ret=recv_n(new_fd,buf,len);
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
			strcat(temp," successfully");
			send(new_fd,temp,strlen(temp),0);
			break;
		}
	}
	return 0;
}

