#include "rsocket.h"

// Global varibles
int socket_fd, ID;				// socket and ID of message
pthread_t X;					// thread
short int **rcv_msg;			// receive message id (hash)
struct sockaddr *srcaddr_list;
unack_t *unack_msg_table;		// Unacknowledged message table (hash)
int unack_count;
rcv_msg_buf rcv_buffer;			// receive buffer queue
pthread_mutex_t buffer_lock;	// mutex locks
pthread_mutex_t unack_lock;

int no_clients;
int close_sockfd_flag;
int no_of_transmissions;
int no_of_sendto;

// encoding a message
int encode(char *buf,int id, char *msg, int len){	// encode the message for transmission
	buf[1] = id >> 24;
	buf[2] = id >> 16;
	buf[3] = id >> 8;
	buf[4] = id;
	int i;
	for(i=0; i<len; i++) buf[i+5] = msg[i];

	return len+5;
}

// Handle Retransmission
void HandleRetransmit(int sockfd){
	int i;
	for(i=0; i<MAX_MSG; i++){
		if(unack_msg_table[i].id>=0 && time(NULL)-unack_msg_table[i].ts>=T){

			// printf("Retransmitted %d %ld\n", unack_msg_table[i].id, time(NULL)); fflush(stdout);
			sendto(sockfd, (char *)unack_msg_table[i].msg, unack_msg_table[i].len, 0, (struct sockaddr *)&unack_msg_table[i].addr, sizeof(unack_msg_table[i].addr));
			
			pthread_mutex_lock(&unack_lock);
			
			unack_msg_table[i].ts = time(NULL);

			no_of_transmissions++;

			pthread_mutex_unlock(&unack_lock);
		}
	}
	return;
}

// Handle Receive of Acknowledgement Message
void HandleAckMsgRecv(int sockfd, char *buf, int len, struct sockaddr srcaddr, socklen_t addrlen){
	
	// printf("handleAckMsgRecv\n"); fflush(stdout);
	int id = buf[1] << 24 | buf[2] << 16 | buf[3] << 8 | buf[4], k=0;

	pthread_mutex_lock(&unack_lock);

	while(id != unack_msg_table[id].id && k <= MAX_MSG){
		k++ ;
		id = (id+1)%MAX_MSG ;
	}

	if(id == unack_msg_table[id].id){ 
		unack_msg_table[id].id = -1;
		unack_count--;
	}

	pthread_mutex_unlock(&unack_lock);

	return;
}

// Handle Receive of Application Message
void HandleAppMsgRecv(int sockfd, char *buf, int len, struct sockaddr srcaddr, socklen_t addrlen){
	
	// printf("handleAppMsgRecv\n"); fflush(stdout);
	int id = buf[1] << 24 | buf[2] << 16 | buf[3] << 8 | buf[4], i, client_id;

	// return without success if the receive buffer is full or max no clients have exceeded capacity
	if(no_clients > MAX_CLIENT) {
		printf("No more clients allowed!");
		return;
	}
	if((rcv_buffer.front+1)%MAX_MSG == rcv_buffer.rear) return;

	for(client_id=0; client_id<no_clients; client_id++){
		if(srcaddr_list[client_id].sa_family == srcaddr.sa_family && !memcmp(srcaddr_list[client_id].sa_data, srcaddr.sa_data, 6)) {
			break;
		}
	}
	if(client_id == no_clients){
		no_clients++;
		srcaddr_list[client_id] = srcaddr;
	}

	int found = rcv_msg[id][client_id];

	if(!found){

		pthread_mutex_lock(&buffer_lock);

		int front = rcv_buffer.front;

		for(i=5; i<len; i++){
			rcv_buffer.q[front].msg[i-5] = buf[i];
		}
		rcv_buffer.q[front].len = len-5;
		rcv_buffer.q[front].addr = srcaddr;
		rcv_buffer.q[front].addrlen = addrlen;
		rcv_buffer.front = (front + 1)%MAX_MSG;

		pthread_mutex_unlock(&buffer_lock);

		rcv_msg[id][client_id] = 1;
	}
	
	char ack[PACKET], temp[1] = "\0";
	ack[0] = 'A';
	encode(ack, id, temp, 1);
	sendto(sockfd, (char *)ack, 6, 0, (struct sockaddr *)&srcaddr, sizeof(srcaddr));
	// printf("ACK %d\n", id);

	return ;
}

// dropMessage
int dropMessage(float p){
	if((float)rand()/RAND_MAX < p)
		return 1;
	return 0;
}

void HandleReceive(int sockfd) {
	char buf[PACKET];
	struct sockaddr srcaddr;
	socklen_t len = sizeof(srcaddr);
	ssize_t rbits;
	rbits = recvfrom(sockfd, (char *)buf, MAXLEN, 0, (struct sockaddr *)&srcaddr, &len);

	if(dropMessage(DROP_PROBABILITY)){
		// printf("drop\n"); fflush(stdout);
		return;
	}

	if(buf[0] == 'A'){
		HandleAckMsgRecv(sockfd, buf, rbits, srcaddr, len);
	}
	else if(buf[0] == 'M'){
		HandleAppMsgRecv(sockfd, buf, rbits, srcaddr, len);
	}
}

// thread running function
void *runThread(void *n){
	int sockfd = socket_fd, r;
	fd_set rset;
	struct timeval tv;

	while(!close_sockfd_flag){
		FD_ZERO(&rset);
		FD_SET(sockfd, &rset);

		tv.tv_sec = T;
		tv.tv_usec = 0;

		r = select(sockfd+1, &rset, 0, 0, &tv);
		if(r < 0){
			perror("Select failed\n");
			exit(0);
		}
		else if(r){
			HandleReceive(sockfd);
		}
		else{
			HandleRetransmit(sockfd);
		}
	}
	pthread_exit(NULL);
}

// socket creation
int r_socket(int domain, int type, int protocol){

	if(type != SOCK_MRP) return -1;

	socket_fd = socket(domain, SOCK_DGRAM, protocol);

	ID = 0;
	int i, j;

	rcv_msg = (short int **)malloc(MAX_MSG * sizeof(short int *));	// alloting rcv_message id table
	for(i=0; i<MAX_MSG; i++) rcv_msg[i] = (short int *)malloc(MAX_CLIENT*sizeof(short int));
	for(i=0; i<MAX_MSG; i++)
		for(j=0; j<MAX_CLIENT; j++)
			rcv_msg[i][j] = 0;

	srcaddr_list = (struct sockaddr *)malloc(MAX_CLIENT*sizeof(struct sockaddr));

	unack_msg_table = (unack_t *)malloc(MAX_MSG * sizeof(unack_t));
	for(i=0; i<MAX_MSG; i++) unack_msg_table[i].id = -1;

	rcv_buffer.q = (msg_t *)malloc(MAX_MSG*sizeof(msg_t));
	for(i=0; i<MAX_MSG; i++) rcv_buffer.q[i].len = -1;

	unack_count = 0;
	close_sockfd_flag = 0;
	no_of_transmissions = 0;
	no_of_sendto = 0;
	no_clients = 0;

	pthread_create(&X, NULL, runThread, NULL);

	if (pthread_mutex_init(&buffer_lock, NULL) != 0 || pthread_mutex_init(&unack_lock, NULL) != 0){
		perror("Mutex lock initiation failed\n");
		exit(0);
	} 

	return socket_fd;
}

// binding the socket
int r_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen){
	return bind(sockfd, addr, addrlen);
}

// sendto function
ssize_t r_sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen){
	
	char *b_ = (char *)buf, b[PACKET];
	int n = encode(b, ID, b_, len); b[0] = 'M';
	int id = ID, k=0, i;

	pthread_mutex_lock(&unack_lock);

	while(unack_msg_table[id].id != -1 && k<=MAX_MSG){
		k++;
		id = (id+1)%MAX_MSG;
	}
	if(k>MAX_MSG) return -1;

	unack_msg_table[id].id = ID;
	unack_msg_table[id].len = n;
	for(i=0; i<n; i++){
		unack_msg_table[id].msg[i] = b[i];
	}
	unack_msg_table[id].addr = *dest_addr;
	unack_msg_table[id].addrlen = addrlen;
	unack_msg_table[id].ts = time(NULL);

	unack_count++;			// increase count of unack msg
	no_of_sendto++;			// increase count of send to
	no_of_transmissions++;	// increase count of transmissions

	pthread_mutex_unlock(&unack_lock);

	sendto(sockfd, (char *)b, n, 0, dest_addr, addrlen);
	ID++;

	return len;
}

// receive message
ssize_t r_recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen){
	
	while(rcv_buffer.front == rcv_buffer.rear) // if q is empty sleep and recheck
		usleep(10000);

	pthread_mutex_lock(&buffer_lock);

	char *m = (char *)buf;
	int rear = rcv_buffer.rear, i;
	for(i=0; i<rcv_buffer.q[rear].len && i<len; i++){
		m[i] = rcv_buffer.q[rear].msg[i];
	}

	*src_addr = rcv_buffer.q[rear].addr;
	*addrlen = rcv_buffer.q[rear].addrlen;

	rcv_buffer.rear = (rcv_buffer.rear + 1)%MAX_MSG;

	pthread_mutex_unlock(&buffer_lock);

	return i;
}

int r_close(int fd){

	while(unack_count != 0) usleep(10000);
	close_sockfd_flag = 1;	// flag to terminate the thread
	pthread_join(X, NULL);	// pthread_join after thread termination

	free(rcv_msg);
	free(srcaddr_list);
	free(unack_msg_table);
	free(rcv_buffer.q);

	return close(fd);
}

double getPerformance(){
	return (double)no_of_transmissions/no_of_sendto;
}