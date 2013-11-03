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
#include "appconst.h"



struct sockaddr_in6 sin6_cli;
int sin6len = sizeof(struct sockaddr_in6);
int seq_number = 0;
int window_start = 0;
int window_end = 0;
int xor_count = XORFREQ;
int window_size;
char buffer[PAYLOADSIZE*XORFREQ];
char xor_buffer[PAYLOADSIZE];
FILE *fp;

int readsync(int socket){
	
	char buf[BUFFSIZE];
	
	int d = recvfrom(socket, &buf, BUFFSIZE, 0, (struct sockaddr*) &sin6_cli, &sin6len);
	if(d== -1){
		perror("Recv failed");
		return -1;
	}else{
		if(buf[0] == (PTYPE_SYN<<5) && buf[1] == 0){
			return 0;
		}else{
			return -1;
		}
	}
}

int sendMsg(int socket, int type, int seq_number, int window){
	char buf[BUFFSIZE];
	if(type == PTYPE_SYN){
		buf[0] = (PTYPE_SYN<<5);
		buf[0] = buf[0]|XORFREQ;
		memset(&(buf[1]), 0,BUFFSIZE - 1);
		return sendto(socket, buf,BUFFSIZE,0,(struct sockaddr *) &sin6_cli, sin6len);	
	}else if(type == PTYPE_ACK){
		buf[0] = (PTYPE_ACK<<5);
		return sendto(socket, buf,BUFFSIZE,0,(struct sockaddr *) &sin6_cli, sin6len);	
	}
	return -1;
}

int readType(char head){
	char format_type = head & 0b11100000;
	if(format_type == PTYPE_DATA<<5) return PTYPE_DATA;
	if(format_type == PTYPE_XOR<<4) return PTYPE_XOR;
}

int readWindow(char head){
	char format_type = head & 0b00011111;
	
	return format_type;
}

int readSeqNumber(char s){
	return s % 256;
		
}

int readLength(char* buf){
	int16_t len;
	memcpy(&len, &buf[LENGTHSTARTPOS], LENGTHBYTESIZE);
	printf("Length : %d\n", len);
	return len;
}

int readAck(int sock){
	char buf[BUFFSIZE];
	int d = recvfrom(sock, &buf, BUFFSIZE, 0, (struct sockaddr*) &sin6_cli, &sin6len);
	if(d== -1){
		perror("Recv failed");
		return -1;
	}else{
		if(buf[0] == (PTYPE_ACK<<5)){
			seq_number++;
			return 0;
		}else{
			return -1;
		}
	}
}

void computeXor(char* buf){
		
		if(xor_count != XORFREQ){
			int i;
			for(i=0; i<PAYLOADSIZE; i++){
				xor_buffer[i] = xor_buffer[i]^buf[4+i];
				printf("%x  ", xor_buffer[i]);
			}
			printf("\n");
		}else{
			memcpy(&xor_buffer, &buf[4], PAYLOADSIZE);
		}
}

void writeBufferToFile(){
	if(fwrite(&buffer[XORFREQ-xor_count], readLength(&buffer[XORFREQ-xor_count]), 1,fp)==-1){
			perror("writing");
			return;
	}
}

int checkXor(char *buf){
		int i;
		for(i = 0; i< PAYLOADSIZE; i++){
			printf("%x %x\n",buf[4+i], xor_buffer[i]);
			if(buf[4+i] != xor_buffer[i]){printf("c'est faux : %d\n",i); return 0;}
		}
		printf("c'est boooon\n");
		/* Xor verified */ 
		writeBufferToFile();
		return 1;
		
}


int receiveFile(int sock){
	fp = fopen("testReceive.c","w");
	
	while(1){
		char buf[BUFFSIZE];
		struct sockaddr_in6 sin6_tmp;
		if(recvfrom(sock, &buf, BUFFSIZE, 0, (struct sockaddr*) &sin6_cli, &sin6len) == -1){
				perror("Connection Lost");
				exit(EXIT_FAILURE);
		}
		printf("received pack n° %d\n", seq_number);
		window_start = (window_start+1)%window_size;
		
		int type = readType(buf[0]);
		if(type == PTYPE_DATA){
			
			int seq = readSeqNumber(buf[1]);
			printf("Seq Number %d %d %d\n", seq, seq_number, xor_count);
			if(seq == seq_number && xor_count !=0){
				memcpy(&buffer[XORFREQ-xor_count], &buf[4], PAYLOADSIZE);
				
				computeXor(buf);
				xor_count--;
				seq_number++;
				printf("coucou %s\n",&buf[4]);
				window_end = (window_end+ 1)%window_size;
				sendMsg(sock, PTYPE_ACK, seq_number, window_end);
				/*if(readLength(&buf[XORFREQ-xor_count]) != PAYLOADSIZE){
						break;
				}*/
			}else{// else discard packet, send last ack again.
				window_start = (window_start+1)%window_size;
				window_end = (window_end+ 1)%window_size;
				sendMsg(sock, PTYPE_ACK, seq_number, window_end);
			}
			
		}else if(type == PTYPE_XOR){
			int seq = readSeqNumber(buf[1]);
			printf("receive xor %d %d  %d\n", seq, seq_number, xor_count);
			if(seq == seq_number && xor_count == 0){
					if(checkXor(buf)){
							
							xor_count = XORFREQ;
							seq_number++;
							window_end = (window_end+ 1)%window_size;
							sendMsg(sock, PTYPE_ACK, seq_number, window_end);
							fclose(fp);
							
					}else{
							// Make choice if XOR is wrong
							xor_count = XORFREQ;
							seq_number++;
							/* SEND SPECIAL ACK.*/
					}
			}
				
		}
	}
	fclose(fp);
		
}



int main(int argc, char**argv){
	struct addrinfo hints, *result;
	int sock;
	window_size = XORFREQ + 1;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s port\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = IPPROTO_UDP;
	
	if(getaddrinfo(NULL, argv[1], &hints, &result)==-1){
		perror("getaddrinfo");
		exit(EXIT_FAILURE);
	}
	
	sock = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
	if(sock == -1){
			perror("socket creation");
			exit(EXIT_FAILURE);
	}
	
	if(bind(sock,(struct sockaddr *) result->ai_addr, sin6len)==-1){
		perror("bind");
		exit(EXIT_FAILURE);
	}
	
	if(readsync(sock)==-1){
		perror("Sync error on reading sync");
		exit(EXIT_FAILURE);
	}
	printf("sync received\n");
	
	if(sendMsg(sock, PTYPE_SYN, 0, 0) ==-1){
		perror("Sync error on first send");
		exit(EXIT_FAILURE);
	}
	printf("sync sent\n");
	
	if(sendMsg(sock, PTYPE_ACK, 0, 0) ==-1){
		perror("Sync error on first ack");
		exit(EXIT_FAILURE);
	}
	printf("ack sent\n");
	
	if(readAck(sock)== -1){
			perror("Acknoledgement reading");
			exit(EXIT_FAILURE);
	}
	printf("ack received\n");
	
	
	printf("Begins reception\n");
	
	if(receiveFile(sock)==-1){
		perror("Error while receiving file");
		exit(EXIT_FAILURE);	
	}
	
	
	freeaddrinfo(result);
	close(sock);
	exit(EXIT_SUCCESS);

	
		
}



