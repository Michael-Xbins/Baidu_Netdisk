#include "../server/include/factory.h"

//循环发送
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
		if(0==ret)	//代表服务器端意外断开（客户端此时要退出）。 注意区别服务器的意外断开（如ctrl+c） 和 退出机制（发10号信号）
		{ 
			return -1;
		}
		total=total+ret;
	}
	return 0;
}

