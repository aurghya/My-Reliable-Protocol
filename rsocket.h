#ifndef R_SOCKET
#define R_SOCKET

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <pthread.h>

#define T 2
#define MAXLEN 101
#define MAX_MSG 101		// Maximum number of messages+1
#define PACKET 111
#define DROP_PROBABILITY 0.20
#define MAX_CLIENT 2
#define SOCK_MRP 59

// user defined structure
// unacknowledged message data type
typedef struct _unack_t{
	int id;
	char msg[PACKET];
	int len;
	struct sockaddr addr;
	socklen_t addrlen;
	time_t ts;
} unack_t;

// data structure for message in the receive buffer
typedef struct _msg_t{
	// int id;
	char msg[MAXLEN];
	int len;
	struct sockaddr addr;
	socklen_t addrlen;
} msg_t;

// structure of message buffer
typedef struct _rcv_msg_buf{
	int front;
	int rear;
	msg_t *q;
} rcv_msg_buf;

int r_socket(int domain, int type, int protocol);
int r_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
ssize_t r_sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen);
ssize_t r_recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen);
int r_close(int fd);
double getPerformance();

#endif
