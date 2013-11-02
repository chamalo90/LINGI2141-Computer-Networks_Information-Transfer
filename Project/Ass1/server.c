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
		buf[0] = buf[0] | XORFREQ;
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
	return format_type >> 5;
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
				xor_buffer[i] = xor_buffer[i] ^ buf[4+i];
			}
		}else{
			memcpy(&xor_buffer, &buf[4], PAYLOADSIZE);
		}
}

void writeBufferToFile(){
	
}

int checkXor(char *buf){
		int i;
		for(i = 0; i< PAYLOADSIZE; i++){
			if(buf[4+i] != xor_buffer[i]) return 0;
		}
		/* Xor verified */ 
		writeBufferToFile();
		return 1;
		
}


int receiveFile(int sock){
	fp = fopen("testReceive.c","w");
	
	while(1){
		printf("receiving pack n° %d\n", seq_number);
		char buf[BUFFSIZE];
		if(recvfrom(sock, &buf, BUFFSIZE, 0, (struct sockaddr*) &sin6_cli, &sin6len) == -1){
				perror("Connection Lost");
				exit(EXIT_FAILURE);
		}
		window_start = (window_start+1)%window_size;
		
		int type = readType(buf[0]);
		if(type == PTYPE_DATA){
			int seq = readSeqNumber(buf[1]);
			if(seq == seq_number && xor_count !=0){
				memcpy(&buffer[XORFREQ-xor_count], &buf[4], PAYLOADSIZE);
				xor_count--;
				computeXor(buf);
				seq_number++;
				window_end = (window_end+ 1)%window_size;
				sendMsg(sock, PTYPE_ACK, seq_number, window_end);
				if(readLength(&buf[XORFREQ-xor_count]) != PAYLOADSIZE){
						break;
				}
			}else{// else discard packet, send last ack again.
				window_start = (window_start+1)%window_size;
				window_end = (window_end+ 1)%window_size;
				sendMsg(sock, PTYPE_ACK, seq_number, window_end);
			}
			
		}else if(type == PTYPE_XOR){
			int seq = readSeqNumber(buf[1]);
			if(seq == seq_number && xor_count == 0){
					if(checkXor(buf)){
							xor_count = XORFREQ;
							seq_number++;
							window_end = (window_end+ 1)%window_size;
							sendMsg(sock, PTYPE_ACK, seq_number, window_end);
							
					}else{
							// Make choice if XOR is wrong
							xor_count = XORFREQ;
							seq_number = seq_number+1;
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
	
	if(sendMsg(sock, PTYPE_SYN, 0, 0) ==-1){
		perror("Sync error on first send");
		exit(EXIT_FAILURE);
	}
	
	/*if(sendMsg(sock, PTYPE_ACK, 0, 0) ==-1){
		perror("Sync error on first ack");
		exit(EXIT_FAILURE);
	}*/
	
/*	if(readAck(sock)== -1){
			perror("Acknoledgement reading");
			exit(EXIT_FAILURE);
	}*/
	
	printf("Begins reception\n");
	if(receiveFile(sock)==-1){
		perror("Error while receiving file");
		exit(EXIT_FAILURE);	
	}
	
	
	
	freeaddrinfo(result);
	close(sock);
	exit(EXIT_SUCCESS);

	//~ memset(&sin6, 0, sin6len);
	//~ sin6.sin6_port = htons(argv[1]);
	//~ sin6.sin6_family = AF_INET6;
	//~ sin6.sin6_addr = in6addr_any;
//~ 
	//~ sock = socket(AF_INET6, SOCK_DGRAM, 0);
//~ 
	//~ if(bind(sock, (struct sockaddr *) &sin6, sin6len)==-1){
		//~ perror("Bind");
		//~ exit(EXIT_FAILURE);		
	//~ }
//~ 
	//~ /* WAITING FOR CLIENT - SYNC PROCESS BEGINS */
	
	
	//~ 
	//~ if(sendMsg(sock, buf, (struct sockaddr *) &sin6, sin6len,PTYPE_ACK, 0, 0)==-1){
		//~ perror("Sync error on ack sync");
		//~ exit(EXIT_FAILURE);
	//~ }
	//~ 
	//~ printf("Sync on %x XOR freq\n", XORFREQ);
	//~ exit(EXIT_SUCCESS);	
		
}


/* source: http://nicolasj.developpez.com/articles/libc/string/ */
char *str_sub (const char *s, unsigned int start, unsigned int end)
{
   char *new_s = NULL;

   if (s != NULL && start < end)
   {
/* (1)*/
      new_s = malloc (sizeof (*new_s) * (end - start + 2));
      if (new_s != NULL)
      {
         int i;

/* (2) */
         for (i = start; i <= end; i++)
         {
/* (3) */
            new_s[i-start] = s[i];
         }
         new_s[i-start] = '\0';
      }
      else
      {
         fprintf (stderr, "Memoire insuffisante\n");
         exit (EXIT_FAILURE);
      }
   }
   return new_s;
}







