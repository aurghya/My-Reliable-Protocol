#include "rsocket.h"

#define PORT1 50118
#define PORT2 50119

int main(){
	int sockfd;
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

	if(r_bind(sockfd, (struct sockaddr *)&destaddr, sizeof(destaddr))<0){
		perror("bind failed\n");
		exit(0);
	}

	while(1){
		char buf[PACKET], msg[MAXLEN], type;
		int len = sizeof(srcaddr);
		int r = r_recvfrom(sockfd, (char *)buf, MAXLEN, 0, (struct sockaddr *)&srcaddr, &len);
		printf("%s",buf);
		fflush(stdout);
	}

	r_close(sockfd);
}