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

int readsync(int socket, char* buf, struct sockaddr *dest_addr, int dest_len){
	if(recvfrom(socket, buf, BUFFSIZE, 0, (struct sockaddr*) &dest_addr, &dest_len)== -1){
		return -1;
	}
	if(buf[0]==(PTYPE_SYN << 5)){
		return 1;
	}el	se{
		return -1;
	}
}

int sendMsg(int socket, char* buf, struct sockaddr *dest_addr, int dest_len, int type, int seq_number, int window){
	if(type == PTYPE_SYN){
		buf[0] = (PTYPE_SYN >> 5) + XORFREQ;
		memset(&(buf[1]), 0,BUFFSIZE - 1);
		return sendto(socket,buf,BUFFSIZE,0,(struct sockaddr *)&dest_addr,dest_len);	
	}else if(type == PTYPE_ACK){
		buf[0] = (PTYPE_ACK >> 5);
	}
}

int main(int argc, char**argv){
	struct addrinfo hints;
	struct addrinfo *result, *rp;
	int sfd, s;
	struct sockaddr* peer_addr;
	socklen_t peer_addr_len;
	ssize_t nread;
	char buf[BUFFSIZE];
	if (argc != 2) {
		fprintf(stderr, "Usage: %s port\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	/* Connect process comes from man page of getaddrinfo (man getaddrinfo)*/
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET6;    /* Allow IPv6 */
	hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
	hints.ai_protocol = 0;          /* Any protocol */
	hints.ai_canonname = NULL;
	hints.ai_addr = NULL;
	hints.ai_next = NULL;

	s = getaddrinfo(NULL, argv[1], &hints, &result);
	if (s != 0) {
		perror("getaddrinfo");
		exit(EXIT_FAILURE);
	}

	/* getaddrinfo() returns a list of address structures.
	   Try each address until we successfully bind(2).
	   If socket(2) (or bind(2)) fails, we (close the socket
	   and) try the next address. */

	for (rp = result; rp != NULL; rp = rp->ai_next) {
		sfd = socket(rp->ai_family, rp->ai_socktype,
				rp->ai_protocol);
		if (sfd == -1)
			continue;

		if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0)
			break;                  /* Success */

		close(sfd);
	}

	if (rp == NULL) {               /* No address succeeded */
		perror("Could not bind");
		exit(EXIT_FAILURE);
	}

	freeaddrinfo(result);           /* No longer needed */
	printf("connected\n");
	/* CONNECTED TO CLIENT - SYNC PROCESS BEGINS */
	if(readsync(sfd, buf, peer_addr, peer_addr_len)==-1){
		perror("Sync error on reading sync");
		exit(EXIT_FAILURE);
	}
	
	if(sendMsg(sfd, buf, peer_addr, peer_addr_len, PTYPE_SYN, 0, 0) ==-1){
		perror("Sync error on first send");
		exit(EXIT_FAILURE);
	}
	
	if(sendMsg(sfd, buf, peer_addr, peer_addr_len,PTYPE_ACK, 0, 0)==-1){
		perror("Sync error on first send");
		exit(EXIT_FAILURE);
	}
	
	printf("Sync on %x XOR freq", XORFREQ);
	exit(EXIT_SUCCESS);	

	

		
}









