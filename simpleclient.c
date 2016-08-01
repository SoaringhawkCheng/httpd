#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[])
{
    int sockfd;
    int len;
    struct sockaddr_in address;
    int result;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr("127.0.0.1");
    address.sin_port = htons(8888);
    len = sizeof(address);
    result = connect(sockfd, (struct sockaddr *)&address, len);

    if (result == -1)
    {
        perror("oops: client1");
        exit(1);
    }
    char* ch = "get / http/1.1\r\n";	
    
	int n = write(sockfd, ch, strlen(ch));
//	close(sockfd);
//	return 0;

	printf("%d\n",n);
	if(n<0)
		perror("write");
	
    char res[1024];

	shutdown(sockfd,SHUT_WR);
    n = read(sockfd, res, sizeof(res));
	if(n<0)
		perror("read");
	if(n==0)
	{
		printf("the EOF...\n");
	}


    printf("char from server = %s\n", res);
    
    close(sockfd);
    exit(0);
}
