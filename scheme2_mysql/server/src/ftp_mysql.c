#include"../include/factory.h"

int query_name(char *name,int sfd,pnode_t task)
{
	char salt[512]={0};
	MYSQL *conn;
	MYSQL_RES *res;
	MYSQL_ROW row;
	const char* server="localhost";
	const char* user="sky";
	const char* password="123456";
	const char* database="test25";//要访问的数据库名称
	char query[300]="select * from user_verify";
	unsigned int t;
	conn=mysql_init(NULL);
	if(!mysql_real_connect(conn,server,user,password,database,0,NULL,0))
	{
		printf("Error connecting to database:%s\n",mysql_error(conn));
		return -1;
	}
	t=mysql_query(conn,query);
	if(t)//数据库区别：mysql_query返回为true代表的是连接失败
	{
		puts(query);
		printf("%s\n",mysql_error(conn));
	}
	else
	{
		int n;
		char buf[512]={0};
		res=mysql_use_result(conn);
		if(res)
		{
			while((row=mysql_fetch_row(res))!=NULL)
			{
				if(!strcmp(name,row[1]))
				{
					bzero(buf,sizeof(buf));
					strcpy(buf,"correct");
					send(sfd,buf,sizeof(buf),0);

					strcpy(salt,row[2]);
					send(sfd,salt,sizeof(salt),0);
					bzero(buf,sizeof(buf));
					recv(sfd,buf,sizeof(buf),0); //收到盐值和密码一起加密的密文

					if(!strcmp(row[3],buf))
					{
						n=1;
						send(sfd,&n,sizeof(n),0);
						strcpy(task->path,row[4]);
						return 1;
					}
					else
					{
						n=-1;
						send(sfd,&n,sizeof(n),0);
					}
					break;
				}
			}
			bzero(buf,sizeof(buf));
			strcpy(buf,"用户不存在");
			send(sfd,buf,sizeof(buf),0);

		}
		mysql_free_result(res);//申请的空间，最后要free掉
	}
	mysql_close(conn); //关闭连接
	return -1;
}

void GenerateSalt(int sfd,char *salt)
{
	char str[STR_LEN + 1] = {0};
	int i,flag;

	srand(time(NULL));//通过时间函数设置随机数种子，使得每次运行结果随机。
	for(i = 0; i < STR_LEN; i ++) 
	{   
		flag = rand()%3;
		switch(flag)
		{   
			case 0:
				str[i] = rand()%26 + 'a'; 
				break;
			case 1:                                                                                   
				str[i] = rand()%26 + 'A'; 
				break;
			case 2:
				str[i] = rand()%10 + '0'; 
				break;
		}   
	}   
	strcpy(salt,str);
	send(sfd,salt,sizeof(salt),0);
}

int login_mysql(int sfd,char *salt,pnode_t task)
{
	char name[512]={0};
	char passwd[512]={0};
	recv(sfd,name,sizeof(name),0);

	if(name[0]<'A'||(name[0]>'Z'&&name[0]<'a')||name[0]>'z')
	{

		int k=-1;
		send(sfd,&k,sizeof(int),0);
		return -1;
	}
	else
	{
		MYSQL *conn;
		MYSQL_RES *res;
		MYSQL_ROW row;
		const char* server="localhost";
		const char* user="sky";
		const char* password="123456";
		const char* database="test25";
		unsigned int t;
		conn=mysql_init(NULL);
		if(!mysql_real_connect(conn,server,user,password,database,0,NULL,0))
		{
			printf("Error connecting to database:%s\n",mysql_error(conn));
		}

		//申请账号时判断用户名是否已经存在
		char query[300]="select * from user_verify"; 
		t=mysql_query(conn,query);
		if(t)//数据库区别：mysql_query返回为true代表的是连接失败
		{
			puts(query);
			printf("%s\n",mysql_error(conn));
		}
		else
		{
			res=mysql_use_result(conn);
			if(res)
			{
				while((row=mysql_fetch_row(res))!=NULL)
				{
					if(!strcmp(name,row[1]))
					{
						int k=0;
						send(sfd,&k,sizeof(k),0);
						return -1;
					}
				}
				int k=1;
				send(sfd,&k,sizeof(k),0);
			}
		}


		recv(sfd,passwd,sizeof(passwd),0);
		char path[512]={0};
		strcpy(path,"/home/sky/Baidu_Netdisk/scheme2_mysql/server/user_catalog/");	//不同服务器要在此处进行配置
		strcat(path,name);
		mkdir(path,0775);
		strcpy(task->path,path);
		puts(task->path);

		char insert[200]="insert into user_verify(user_id,user_name,salt,encode,path) values(null";
		sprintf(insert,"%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",insert, "," , "'" , name , "'" , "," , "'" , salt , "'" , "," , "'" , passwd , "'" , "," , "'" , path , "'", ")");
		puts(insert);
		conn=mysql_init(NULL);
		if(!mysql_real_connect(conn,server,user,password,database,0,NULL,0))
		{
			printf("Error connecting to database:%s\n",mysql_error(conn));
		}
		t=mysql_query(conn,insert);  //向conn 连接，发送query 插入指令
		if(t)
		{
			printf("Error making insert:%s\n",mysql_error(conn));
		}else{
			printf("insert success\n");
		}
		mysql_close(conn);
	}
	return 0;
}

