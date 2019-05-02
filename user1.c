#include <stdio.h>
#include <string.h>
#include "rsocket.h"

#define PORT1 50120
#define PORT2 50119

int main(){
	int sockfd, len;
	struct sockaddr_in srcaddr, destaddr;
	memset(&srcaddr, 0, sizeof(srcaddr));
	memset(&destaddr, 0, sizeof(destaddr));

	sockfd = r_socket(AF_INET, SOCK_MRP, 0);
	if(sockfd < 0){
		perror("socket creation failed\n");
		exit(0);
	}

	srcaddr.sin_family = AF_INET;
	srcaddr.sin_port = htons(PORT1);
	srcaddr.sin_addr.s_addr = INADDR_ANY;

	destaddr.sin_family = AF_INET;
	destaddr.sin_port = htons(PORT2);
	destaddr.sin_addr.s_addr = INADDR_ANY;

	if(r_bind(sockfd, (struct sockaddr *)&srcaddr, sizeof(srcaddr))<0){
		perror("bind failed\n");
		exit(0);
	}

	char str[102], buf[2]; buf[1]='\0';
	printf("Enter a string : ");
	fgets(str, 102, stdin);
	str[strlen(str)-1] = '\0';

	int i=0;
	while(1){
		buf[0] = str[i++];
		if(buf[0] == '\0') break;
		r_sendto(sockfd, (char *)buf, strlen(buf)+1, 0, (struct sockaddr *)&destaddr, sizeof(destaddr));
	}

	r_close(sockfd);
	printf("%f\n", getPerformance());

	return 0;
}
