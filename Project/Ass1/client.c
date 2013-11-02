/* LINGI2141 - Project 1 
 * Authors : Julien Colmonts - Benoit Baufays 
  */
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include "appconst.h"

struct sockaddr sin6;
struct sockaddr_in6 sin6_serv;
int sin6len = sizeof(struct sockaddr_in6);
int seq_number = 0;
int window_start = 0;
int window_end = 0;
int xor_count;
int xorfreq;
int window_size;
char *buffer;
char xor_buffer[BUFFSIZE];
int sock;
FILE *fp;
sem_t sem;
int finished=0;

char* packet_path = "./client.c";


int sendMsg(int socket, char* buf, struct sockaddr* dest_addr, int dest_len, int type, int seq_number, int window){
	if(type == PTYPE_SYN){
		memset(&(buf[0]), 0,BUFFSIZE);
		buf[0] = PTYPE_SYN<<5; 
		return sendto(socket, buf,BUFFSIZE,0,(struct sockaddr *) dest_addr,dest_len);	
	}else if(type == PTYPE_ACK){
		memset(&(buf[0]), 0,BUFFSIZE);
		buf[0] = (PTYPE_ACK<<5);
		return sendto(socket, buf,BUFFSIZE,0,(struct sockaddr *) dest_addr,dest_len);	
	}else if(type == PTYPE_DATA){
		buf[0] = (PTYPE_DATA<<5) | window_end;
		buf[1] = seq_number++;
	}
}

int readsync(int socket, struct sockaddr* dest_addr, int dest_len){
	char buf[BUFFSIZE];
	int d = recvfrom(socket, buf, BUFFSIZE, 0, (struct sockaddr*) &dest_addr, &dest_len);
	if(d== -1){
		return -1;
	}else{
		
		if((buf[0]& 0b11100000) ==PTYPE_SYN<<5 ){	
			return (buf[0] & 0b00011111);
		}else{
			return -1;
		}
	}
	
}

void computeXor(char* buf){
		if(xor_count != XORFREQ){
			int i;
			for(i=0; i<PAYLOADSIZE; i++){
				xor_buffer[i] = xor_buffer[i] ^ buf[4+i];
			}
		}else{
			memcpy(&xor_buffer, &buf[4], PAYLOADSIZE);
		}
}

int readType(char head){
	char format_type = head & 0b11100000;
	return format_type >> 5;
}

int readWindow(char head){
	char format_type = head & 0b00011111;
	return format_type;
}

int readSeqNumber(char s){
	return s % 256;
		
}

void* packet_send(void* data){
	while(1){
		printf("sending pack n° %d\n", seq_number);
		if(xor_count!=0){
			int buff_start = (XORFREQ-xor_count)*BUFFSIZE;
			fread(&buffer[buff_start + 4], 512, 1, fp);
			
			sem_wait(&sem);
			window_end = (window_end + 1) % window_size;
			if(sendMsg(sock, &buffer[buff_start],(struct sockaddr*) &sin6, sin6len, PTYPE_DATA, seq_number, window_end)==-1){
					perror("connection lost");
					exit(EXIT_FAILURE);
			}
			computeXor(&buffer[buff_start]);
			seq_number++;
			xor_count--;
		}else{
			sem_wait(&sem);
			window_end = (window_end + 1) % window_size;
			if(sendMsg(sock, xor_buffer,(struct sockaddr*) &sin6, sin6len, PTYPE_XOR, seq_number, window_end)==-1){
					perror("connection lost");
					exit(EXIT_FAILURE);
			}
			seq_number++;
			xor_count--;
		}
	}
}



void* ack_receive(void* data){
	while(!finished){
		char buf[BUFFSIZE];
		if(recvfrom(sock, buf, BUFFSIZE, 0, (struct sockaddr*) &sin6_serv, &sin6len)==-1){
					perror("connection lost");
					exit(EXIT_FAILURE);
		}
		if(readType(buf[0]== PTYPE_ACK)){
			int seq = readSeqNumber(buf[1]);
			if(seq==seq_number+1){
					seq_number++;
			}
		}
		sem_post(&sem);
	}
}

int saveFile(){
	fp = fopen("./client.c","r");
	
	pthread_t packet_sender, ack_receiver;
	xor_count = xorfreq;
	sem_init(&sem, 0, window_size);
	pthread_create(&packet_sender,NULL, packet_send, NULL);
	pthread_create(&ack_receiver,NULL, ack_receive, NULL);
	pthread_join(packet_sender, NULL);
	finished=1;
	pthread_join(ack_receiver,NULL);
}

int main(int argc, char *argv[])
{

	struct addrinfo hints,*result;
	char sync_buf[BUFFSIZE];

	if (argc < 3) {
		fprintf(stderr, "Usage: %s host port\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = IPPROTO_UDP;
	
	
	if(getaddrinfo(argv[1], argv[2], &hints, &result)==-1){
		perror("getaddrinfo");
		exit(EXIT_FAILURE);
	}
	
	sock = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
	if(sock == -1){
			perror("socket creation");
			exit(EXIT_FAILURE);
	}
	
	
	
	//~ sendto(sock, "coucou",6,0,(struct sockaddr*) result->ai_addr, result->ai_addrlen);
	//~ char buf[6];
	//~ printf("fini");
	//~ fflush(stdout);
	//~ recvfrom(sock, &buf, 6, 0, (struct sockaddr*) &sin6, &sin6len);
	//~ printf("fini : %s\n", buf);
	sin6 =*result->ai_addr;
	if(sendMsg(sock, sync_buf,(struct sockaddr*) &sin6, sin6len , PTYPE_SYN, 0, 0) ==-1){
		perror("Sync error on first send");
		exit(EXIT_FAILURE);
	}

	xorfreq = readsync(sock, (struct sockaddr*) &sin6_serv, sin6len);
	if(xorfreq == -1) {
		perror("Xor freq reception");
		exit(EXIT_FAILURE);	
	}
	window_size = xorfreq + 1;
	seq_number =1;
	buffer = malloc(sizeof(char) * BUFFSIZE * xorfreq);
	/* RECEPTION PROCESS */
	printf("Begins sending op\n");
	if(saveFile()==-1){
		perror("transmission failed");
		exit(EXIT_FAILURE);
	}
	
	
	printf("finished\n");
	
	freeaddrinfo(result);
	close(sock);
	exit(EXIT_SUCCESS);
	
	//~ sock = socket(AF_INET6, SOCK_DGRAM,0);
	//~ if(sock==-1){
		//~ perror("Socket creation failed");
		//~ exit(EXIT_FAILURE);
	//~ }
	

	//~ /* Obtain address(es) matching host/port */
	//~ sin6len = sizeof(struct sockaddr_in6);
	//~ memset(&sin6, 0, sin6len);
	//~ sin6.sin6_port = htons(0);
	//~ sin6.sin6_family = AF_INET6;
	//~ sin6.sin6_addr = in6addr_any;
//~ 
//~ 
	//~ if(bind(sock, (struct sockaddr *)&sin6, sin6len) == -1){
		//~ perror("bind ");
		//~ exit(EXIT_FAILURE);
	//~ }
//~ 
//~ 
	//~ memset(&hints, 0, sizeof(struct addrinfo));
//~ 
	//~ hints.ai_family = AF_INET6;
	//~ hints.ai_socktype = SOCK_DGRAM;
	//~ hints.ai_protocol = IPPROTO_UDP;
//~ 
	//~ if (getaddrinfo(argv[1], argv[2], &hints, &rp) != 0) {
		//~ perror("getaddrinfo");
		//~ exit(EXIT_FAILURE);
	//~ }
	//~ 
//~ /* SYNC PROCESS */
//~ 
	

	//~ 
	//~ if(sendMsg(sock, sync_buf, (struct sockaddr*) rp->ai_addr, sin6len , PTYPE_ACK, 0, 0) == -1){
		//~ perror("Sync error on xor freq acknolegement");
		//~ exit(EXIT_FAILURE);
	//~ }
	//~ printf("Sync on %s XOR freq\n", sync_buf);
//~ 
//~ /* RECEPTION PROCESS */
	//~ if(saveFile("./Enoncé",sock, (struct sockaddr*) rp->ai_addr, sin6len, xorfreq)==-1){
		//~ perror("transmission failed");
		//~ exit(EXIT_FAILURE);
	//~ }
	//~ 
//~ /* FREE PROCESS */
//~ 
	//~ freeaddrinfo(rp);
	//~ rp = NULL;
	//~ close(sock);
	//~ exit(EXIT_SUCCESS);
	
}

