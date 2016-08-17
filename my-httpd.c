//Author : TangMing 

#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h> 
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>

#define gettid() syscall(__NR_gettid)

#include "vector.h"

static vector* fds = NULL;

#define SERVER_NAME "Server: tm-httpd\r\n"

enum method_e{
	POST=1,
	GET,
	DELETE
};
typedef enum method_e method_t;

enum version_e{
	HTTP1_0=1,
	HTTP1_1
};
typedef enum version_e version_t;

enum parser_state_e{
	kExpectedMethod = 0,
	kExpectedUri,
	kExpectedVersion,
	kExpectedHeader,
	kExpectedContent,
	kSuccess
};
typedef enum parser_state_e parser_state_t;

struct http_conn_s{
	method_t request_type;
	char* uri;
	version_t version;
	parser_state_t state;

	char* buffer;
	size_t capacity;
	size_t size;
	char* parser_index;
};
typedef struct http_conn_s http_conn_t;


int web_read(int fd, char* content, int len);
int web_write(int fd, char* content, int len);



void err_exit(char* a)  
{						
	perror(a);
	if(fds)
		destroy(fds);			
	exit(EXIT_FAILURE);	
}			

void signal_handle(int signo)
{
	if( signo == SIGINT )
	{
		printf("close the sever...\n");
		exit(EXIT_FAILURE);
	}

}

void log_err(int fd,char *s)
{	
	write(fd, s, strlen(s));
}


http_conn_t* create_request_paser()
{
	http_conn_t* http = calloc(1,sizeof(http_conn_t));
	http->buffer = malloc(1024);
	http->capacity = 1024;
	http->parser_index = http->buffer;

	return http;
}

void destroy_http_conn(http_conn_t* http)
{
	assert(http->capacity>0);
	free(http->buffer);
	free(http);
}

parser_state_t request_parser(http_conn_t* http)
{
	char *begin = http->parser_index;
	char* space = NULL;
	char* crlf = NULL;

	switch(http->state){
		case kExpectedMethod: goto method;
		case kExpectedUri: goto uri;
		case kExpectedVersion: goto version;
		case kExpectedHeader: goto header;
		case kExpectedContent: goto content;
		default: goto out;
	}
method:
	space = strstr(begin," ");
	if(!space)
		goto out;
	*space = '\0';
	if(strcasecmp(begin,"post") == 0) 
	{
		http->request_type = POST;
	}
	else
	{
		if(strcasecmp(begin,"get") == 0) 
		{
			http->request_type = GET;
		}
	}
	http->state = kExpectedUri;
	begin = ++space;

uri:
	space = strstr(begin," ");
	if(!space)
		goto out;
	*space = '\0';
	http->uri = (char*)malloc(strlen(begin)+1);
	strcpy(http->uri,begin);
	begin = ++space;
	http->state = kExpectedVersion;

version:
	crlf = strstr(begin,"\r\n");
	if(!crlf)
		goto out;
	*crlf = '\0';
	if(strcasecmp(begin,"http/1.0")==0)
	{
		http->version = HTTP1_0;
	}
	else{
		if(strcasecmp(begin,"http/1.1")==0)
			http->version = HTTP1_1;
	}
	http->state = kExpectedHeader;	
	begin = crlf + 2;

header:
content:
out:
	http->parser_index = begin;
	return http->state;
}

void err_method(int fd)
{
	char buff[512];
	sprintf(buff,"HTTP/1.0 501 Method Not Implemented\r\n");
	web_write(fd,buff,strlen(buff));
	sprintf(buff,SERVER_NAME);
	web_write(fd,buff,strlen(buff));
	sprintf(buff,"Content-Type: text/html\r\n");
	web_write(fd,buff,strlen(buff));
	sprintf(buff,"\r\n");
	web_write(fd,buff,strlen(buff));
	sprintf(buff,"<HTML><HEAD><TITLE>Method Not Implemented\r\n");
	web_write(fd,buff,strlen(buff));
	sprintf(buff,"</TITLE></HEAD>\r\n");
	web_write(fd,buff,strlen(buff));
	sprintf(buff,"<BODY><P>HTTP request method not supported.\r\n");
	web_write(fd,buff,strlen(buff));
	sprintf(buff,"</BODY></HTML>\r\n");
	web_write(fd,buff,strlen(buff));
}

void not_found(int fd)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
    web_write(fd, buf, strlen(buf));
    sprintf(buf, SERVER_NAME);
    web_write(fd, buf, strlen(buf));
    sprintf(buf, "Content-Type: text/html\r\n");
    web_write(fd, buf, strlen(buf));
    sprintf(buf, "\r\n");
    web_write(fd, buf, strlen(buf));
    sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
    web_write(fd, buf, strlen(buf));
    sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
    web_write(fd, buf, strlen(buf));
    sprintf(buf, "your request because the resource specified\r\n");
    web_write(fd, buf, strlen(buf));
    sprintf(buf, "is unavailable or nonexistent.\r\n");
    web_write(fd, buf, strlen(buf));
    sprintf(buf, "</BODY></HTML>\r\n");
    web_write(fd, buf, strlen(buf));
}

void send_file(int fd,char* path)
{
	char content[512];
	int html = open(path,O_RDWR);
	int num = read(html,content,sizeof(content));
	while(num > 0)
	{
		web_write(fd,content,num);
		num = read(html,content,sizeof(content));
	}
	close(html);
}

int web_read(int fd, char* content, int len)
{
	int num = read(fd, content, sizeof(content));
	if(num == 0)
	{
		printf("read: peer has closed connection...\n");
		goto out;
	}
	if(num < 0)
	{
		perror("read");
	}
out:
	return num;
}


int web_write(int fd,char* content,int len){
	int res = write(fd,content,len);
	if(res < 0)
	{
		if(errno == EPIPE)
		{
			printf("write: peer has close connection...\n");
			goto out;
		}
		perror("write");
	}

out:
	return res;
}

int response(int fd,char* path)
{
	if(access(path,F_OK|R_OK) == 0)
	{
		char info[128];
		sprintf(info,"HTTP/1.1 200 OK\r\n");
		web_write(fd,info,strlen(info));
		sprintf(info,"Content-Type: text/html\r\n");
		web_write(fd,info,strlen(info));
		sprintf(info,SERVER_NAME);
		web_write(fd,info,strlen(info));
		sprintf(info,"\r\n");
		web_write(fd,info,strlen(info));

		send_file(fd,path);
	}
	else
	{
		not_found(fd);
	}
}

void* bg_serve(void* arg)
{
	assert(fds);
	printf("bg thread pid:%u tid:%u pgrp:%u\n",getpid(),gettid(),getpgrp());

	int log = open("httpd_log",O_APPEND|O_CREAT|O_RDWR);
	if(log == -1)
		err_exit("open");

	char content[1024];
	int num = 0;
	int fd;

	while(1){
		//条件变量的使用
		pthread_mutex_lock(&fds->mu);
		while(fds->count == 0)
		{
			pthread_cond_wait(&fds->cond,&fds->mu);
		}
		assert(fds->count!=0);
		fd = fds->vec[0];
		fd_remove(fds,fd);
		pthread_mutex_unlock(&fds->mu);
		
		http_conn_t* http = create_request_paser();

readstart:
		assert(http->capacity > http->size);

		//简单的http协议请求，只分析请求报头
		num = web_read(fd, http->buffer + http->size, http->capacity - http->size);
		if(num <= 0)
			goto reset;

		http->size += num;
		parser_state_t stat = request_parser(http);
		if(stat != kExpectedHeader)
			goto readstart;

		if(http->request_type == 0)
			err_method(fd);
		else
		{
			if(http->request_type == POST)
			{

			}
			else{	
				if(http->request_type == GET)
				{
					char path[128];
					sprintf(path,"resources");		
					if(http->uri)
						strcat(path,http->uri);
					size_t length = strlen(path);
					if(path[length-1] == '/')
						strcat(path,"index.html");

		//			sleep(3);
					response(fd,path);
				}
			}
		}

reset:
		destroy_http_conn(http);
		shutdown(fd, SHUT_RDWR);
		close(fd);
	}
	

	close(log);
	printf("pthread end...\n");

	pthread_exit(0);
}

int main(int argc,char* argv[])
{
	sigignore(SIGHUP);
	sigignore(SIGPIPE);
	if( signal(SIGINT,signal_handle) == SIG_ERR )
		err_exit("signal");
	
	if(chdir("/home/web/my-httpd")<0)
		err_exit("chdir");

	int sock_server;
	sock_server = socket(AF_INET,SOCK_STREAM,0);
	u_short port = 8888;
	
	if(sock_server < 0)
		err_exit("create socket");

	struct sockaddr_in addr;
	socklen_t size = sizeof(addr);
	memset(&addr,0,size);
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(port);

	int use = 1;
	if(setsockopt(sock_server, 
				SOL_SOCKET,
				SO_REUSEADDR,
				&use,sizeof(use))<0)
		err_exit("setsockopr");

	if(bind(sock_server,
		(struct sockaddr*)(&addr),size))
		err_exit("bind");


	if(getsockname(sock_server, (struct sockaddr*)&addr, &size))
		err_exit("getsockname");

	port = ntohs(addr.sin_port);

	if(listen(sock_server,10))
		err_exit("listen");

	printf("web server is running! #port:%d\n",port);

	fds = init(5);

	pthread_attr_t attr;
	pthread_attr_init(&attr);
	assert(
		pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED) == 0 );

	pthread_t thread1;
	if( pthread_create(&thread1, &attr, bg_serve, NULL) < 0 )
		err_exit("pthread_create");
	pthread_t thread2;
	if( pthread_create(&thread2, &attr, bg_serve, NULL) < 0 )
		err_exit("pthread_create");

	printf("main process pid:%u tid:%u pgrp:%u\n",getpid(),gettid(),getpgrp());
	while(1)
	{
		struct sockaddr_in client_name;
		socklen_t size = sizeof(client_name);
		memset(&client_name,0,size);

		int sock_client = accept(sock_server, (struct sockaddr*)&client_name, &size);
		//这个地方返回的是客户端的地址和端口
		//inet_addr inet_aton等等

		if(sock_client < 0)
			err_exit("accept");

		u_short client_port = ntohs(
			client_name.sin_port);
		char *client_addr = inet_ntoa(client_name.sin_addr);
		printf("client IP:%s, PORT:%d\n",client_addr,client_port);

	//	int val;
	//	val = fcntl(sock_client, F_GETFL);
	//	val |= O_NONBLOCK;
	//	fcntl(sock_client, F_SETFL, val);

		//这里有两个消费者，故需要每次生产都唤醒，以防由于cpu调度而使资源暂时没有被取走导致后续的生产不在唤醒其他线程
		pthread_mutex_lock(&fds->mu);
		fd_push(fds,sock_client);
		pthread_cond_signal(&fds->cond);
		pthread_mutex_unlock(&fds->mu);
	}

	close(sock_server);
	return 0;
}
