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

char buffer[PAYLOADSIZE*XORFREQ];
char xor_buffer[PAYLOADSIZE];
int msg_rvcd = 0;
int last_length = PAYLOADSIZE;
char* file_path;
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
	return (uint8_t) format_type >> 5;
}

int readWindow(char head){
	char format_type = head & 0b00011111;
	
	return format_type;
}

int readSeqNumber(char s){
	return (uint8_t)s % 256;
		
}

int readLength(char* buf){
	//int16_t len = ((uint8_t) buf[LENGTHSTARTPOS]) << 8 | (uint8_t) buf[LENGTHSTARTPOS+1];
	uint8_t left = (uint8_t) buf[LENGTHSTARTPOS];
	uint8_t right = (uint8_t) buf[LENGTHSTARTPOS+1];
	uint16_t len = left << 8 | right;
	printf("Length : %x %x %u %u %d\n",buf[LENGTHSTARTPOS], buf[LENGTHSTARTPOS+1], left, right, len);
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

//~ void computeXor(char* buf){
		//~ 
		//~ if(xor_count != XORFREQ){
			//~ int i;
			//~ for(i=0; i<PAYLOADSIZE; i++){
				//~ xor_buffer[i] = xor_buffer[i]^buf[4+i];
			//~ }
		//~ }else{
			//~ memcpy(&xor_buffer, &buf[4], PAYLOADSIZE);
		//~ }
//~ }


void computeXor(char* buf){
	int j;
	for (j =0; j<XORFREQ; j++){
		if(j!=0){
			int i;
			for(i=0; i<PAYLOADSIZE; i++){
				xor_buffer[i] = xor_buffer[i] ^ buf[(j*PAYLOADSIZE)+i];
			}
			
		}else{
			memcpy(&xor_buffer, buf, PAYLOADSIZE);
		}
	}
}

void writeBufferToFile(){
	if(fwrite(&buffer, PAYLOADSIZE*(msg_rvcd-1), 1,fp)==-1){
			perror("writing");
			return;
	}
	if(fwrite(&buffer[PAYLOADSIZE*(msg_rvcd-1)], last_length, 1,fp)==-1){
			perror("writing");
			return;
	}
}

int checkXor(char *buf){
		int i;
		for(i = 0; i< PAYLOADSIZE; i++){
			if(buf[4+i] != xor_buffer[i]){ return 0;}
		}

		/* Xor verified */ 
		writeBufferToFile();
		return 1;
		
}


int receiveFile(int sock){
	fp = fopen(file_path,"w");
	if(fp == NULL)
	{
			perror("open");
			exit(EXIT_FAILURE);
	}
	while(1){
		char buf[BUFFSIZE];
		if(recvfrom(sock, &buf, BUFFSIZE, 0, (struct sockaddr*) &sin6_cli, &sin6len) == -1){
				perror("Connection Lost");
				exit(EXIT_FAILURE);
		}
		printf("received pack n° %d\n", seq_number);
		window_start = (window_start+1)%MAXWINDOWSIZE;
		
		int type = readType(buf[0]);
		if(type == PTYPE_DATA){
			
			int seq = readSeqNumber(buf[1]);
			if(seq == seq_number && xor_count !=0){
				memcpy(&buffer[(XORFREQ-xor_count)*PAYLOADSIZE], &buf[4], PAYLOADSIZE);
				msg_rvcd++;
				xor_count--;
				
				seq_number = (seq_number+1)%256;
				window_end = (window_end+ 1)%MAXWINDOWSIZE;
				sendMsg(sock, PTYPE_ACK, seq_number, window_end);
				last_length =readLength(buf);
				if(last_length<512){
						xor_count=0;
				}
				
			}else{// else discard packet, send last ack again.
				sendMsg(sock, PTYPE_ACK, seq_number, window_end);
			}
			
		}else if(type == PTYPE_XOR){
			computeXor(buffer);
			int seq = readSeqNumber(buf[1]);
			if(seq == seq_number && xor_count == 0){
					if(checkXor(buf)){
							xor_count = XORFREQ;
							seq_number= (seq_number+1)%256;
							window_end = (window_end+ 1)%MAXWINDOWSIZE;
							
							sendMsg(sock, PTYPE_ACK, seq_number, window_end);
							printf("Xor corr, nb: %d seq nb: %d\n",msg_rvcd, seq_number);
							if(last_length<512){return 0;}
							msg_rvcd=0;
							
					}else{
							// Make choice if XOR is wrong
							xor_count = XORFREQ;
							seq_number= (seq_number+1)%256;
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

	if (argc < 3) {
		fprintf(stderr, "Usage: %s port filename\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	file_path =  argv[2];
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
	
	if(sendMsg(sock, PTYPE_SYN, 0, 0) ==-1){
		perror("Sync error on first send");
		exit(EXIT_FAILURE);
	}
	
	if(sendMsg(sock, PTYPE_ACK, 0, 0) ==-1){
		perror("Sync error on first ack");
		exit(EXIT_FAILURE);
	}
	
	if(readAck(sock)== -1){
			perror("Acknoledgement reading");
			exit(EXIT_FAILURE);
	}
	if(receiveFile(sock)==-1){
		perror("Error while receiving file");
		exit(EXIT_FAILURE);	
	}
	
	freeaddrinfo(result);
	close(sock);
	exit(EXIT_SUCCESS);

	
		
}



