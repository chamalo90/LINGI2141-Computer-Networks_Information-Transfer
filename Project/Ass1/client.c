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
#include <poll.h>
#include "appconst.h"

struct sockaddr sin6;
struct sockaddr_in6 sin6_serv;
int sin6len = sizeof(struct sockaddr_in6);
int seq_number = 0;
int window_start = 1;
int window_end = 1;
int xor_count;
int xorfreq;

char *buffer;
char xor_buffer[PAYLOADSIZE];
int sock;
sem_t sem;
int finished=0;
int last_pack=0;
pthread_t *last;
int acknoledged = 0;
char* file_path= NULL;
FILE *fp=NULL;


void* poll_timer(void* data){
	int seq;
	memcpy(&seq, data, sizeof(int));
	struct pollfd fds;
	fds.fd = sock;
	fds.events = POLLIN;
	if(poll(&fds,1,2000)==0){
		printf("FAIL %d %d %d", acknoledged, seq, seq_number);
		if(acknoledged<seq){
		int calculdeben = seq - ((seq-1)%(xorfreq+1));
		long calculdeju = calculdeben - (calculdeben/(xorfreq+1)) -1;
		fseek(fp, -((calculdeju)*PAYLOADSIZE), SEEK_CUR);
		seq_number = calculdeben;
		}
	}
	
}


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
				pthread_t thread;
		last =&thread;
		pthread_create(&thread,NULL, poll_timer,&seq_number);
		return sendto(socket, buf,BUFFSIZE,0,(struct sockaddr *) dest_addr,dest_len);	
	}else if(type == PTYPE_XOR){
				pthread_t thread;
		last =&thread;
		pthread_create(&thread,NULL, poll_timer,&seq_number);
		char buffer2[BUFFSIZE];
		memset(&buffer2[0], 0 , BUFFSIZE);
		buffer2[0] = (PTYPE_XOR<<5);
		buffer2[1] = seq_number;
		char left = PAYLOADSIZE >> 8;
		char right= PAYLOADSIZE % 256;
		memcpy(&buffer2[LENGTHSTARTPOS], &left, 1);
		memcpy(&buffer2[LENGTHSTARTPOS+1], &right, 1);
		memcpy(&(buffer2[4]), &buf[0], PAYLOADSIZE);
		
		/*
		printf("Seq %x\n", buffer[1]);
		printf("Length %x%x\n", buffer[2], buffer[3]);
		printf("Payload %x \n %x \n", buffer[4], xor_buffer[0]);*/
		
		return sendto(socket, buffer2,BUFFSIZE,0,(struct sockaddr *) dest_addr,dest_len);	
		
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
	int j;
	for (j =0; j<xorfreq; j++){
		if(j!=0){
			int i;
			for(i=0; i<PAYLOADSIZE; i++){
				xor_buffer[i] = xor_buffer[i] ^ buf[(j*BUFFSIZE)+4+i];
			}
		}else{
			memcpy(&xor_buffer, &buf[4], PAYLOADSIZE);
		}
	}
}

int readType(char head){
	char format_type = head & 0b11100000;
	return (uint8_t) format_type >> 5;
}
 
int readWindow(char head){
	char format_type = head & 0b00011111;
	return format_type;
}

int readSeqNumber(char s){
	return s % 256;
		
}

void* packet_send(void* data){
	
	printf("fopen %s\n", file_path);
	fp = fopen(file_path,"r");
	//fp = NULL;
	perror("fopen");
	/*if(fp == NULL){
		perror("File could not be opened");
		exit(EXIT_FAILURE);
	}*/

	while(1){
		printf("sending pack n° %d\n", seq_number);
		if(xor_count!=0){
			
			int buff_start = (xorfreq-xor_count)*BUFFSIZE;
			memset(&buffer[buff_start], 0 , BUFFSIZE);
			buffer[buff_start] = (PTYPE_DATA << 5) | window_end;
			
			buffer[buff_start+1] = seq_number;
			int16_t byte_count = fread(&buffer[buff_start + 4], 1, PAYLOADSIZE, fp);
			char left = byte_count >> 8;
			char right = byte_count % 256;
			
			
			if(byte_count == -1){
					perror("fread");
			}
			memcpy(&buffer[buff_start+2], &left, 1);
			memcpy(&buffer[buff_start+3], &right, 1);
			printf("Count fread:%d %x %x\n", byte_count, buffer[buff_start+2], buffer[buff_start+3]);
			uint16_t test =  buffer[buff_start+2]<<8 | (u_int8_t) buffer[buff_start+3];
			printf("buf length : %u\n",test);
	
			
			sem_wait(&sem);
			window_end = (window_end + 1) % MAXWINDOWSIZE;
			
			if(sendMsg(sock, &buffer[buff_start],(struct sockaddr*) &sin6, sin6len, PTYPE_DATA, seq_number, window_end)==-1){
					perror("connection lost");
					exit(EXIT_FAILURE);
			}
			
			seq_number= (seq_number+1)%256;
			if(byte_count < 512){ xor_count = 0; last_pack = 1;}
			else xor_count--;
		}else{
			sem_wait(&sem);
			window_end = (window_end + 1) % MAXWINDOWSIZE;
			printf("send xor\n");
			computeXor(buffer);
			if(sendMsg(sock, xor_buffer,(struct sockaddr*) &sin6, sin6len, PTYPE_XOR, seq_number, window_end)==-1){
					perror("connection lost");
					exit(EXIT_FAILURE);
			}
			seq_number= (seq_number+1)%256;
			if(last_pack){ return 0;}
			xor_count= xorfreq;
		}
	}
}



void* ack_receive(void* data){
	while(!finished){
		char buf[BUFFSIZE];
		if(recvfrom(sock, buf, BUFFSIZE, 0, NULL, NULL)==-1){
					perror("connection lost");
					exit(EXIT_FAILURE);
		}
		if(readType(buf[0]== PTYPE_ACK)){
			int seq = readSeqNumber(buf[1]);
			if(seq==seq_number+1){
					window_start = (window_start + 1) % MAXWINDOWSIZE;
					seq_number = (seq_number+1)%256;
					acknoledged = (acknoledged+1)%256;
			}
		}
		sem_post(&sem);
	}
}

int saveFile(){
	
	
	
	pthread_t packet_sender, ack_receiver;
	
	xor_count = xorfreq;
	
	sem_init(&sem, 0, 4);
	pthread_create(&packet_sender,NULL, packet_send, NULL);
	pthread_create(&ack_receiver,NULL, ack_receive, NULL);
	pthread_join(packet_sender, NULL);
	finished=1;
	pthread_join(ack_receiver,NULL);
	sem_destroy(&sem);
}

int main(int argc, char *argv[])
{

	struct addrinfo hints,*result;
	char sync_buf[BUFFSIZE];

	if (argc < 4) {
		fprintf(stderr, "Usage: %s host port filename\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	file_path = argv[3];
	printf("file %s %s\n", file_path, argv[3]);
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
	
	
	/* Test Connection */
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
	if(sendMsg(sock, sync_buf,(struct sockaddr*) &sin6, sin6len , PTYPE_ACK, 0, 0) ==-1){
		perror("Sync error on syn ack");
		exit(EXIT_FAILURE);
	}
	
	seq_number =1;
	
	
	char b[BUFFSIZE * xorfreq];
	
	buffer = b;
	
	/* RECEPTION PROCESS */
	
	
	if(saveFile()==-1){
		perror("transmission failed");
		exit(EXIT_FAILURE);
	}
	
	freeaddrinfo(result);
	close(sock);
	exit(EXIT_SUCCESS);
	
	
}

