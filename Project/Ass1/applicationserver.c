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

#define PTYPE_DATA (0x1)
#define PTYPE_ACK (0x2)
#define PTYPE_SYN (0x3)
#define PTYPE_XOR (0x4)

#define BUFFSIZE 544
#define XORFREQ 0x03

int read_sync(char *buf){
	if(buf[0]==(PTYPE_SYN << 5)){
		return 1;
	}else{
		return -1;
	}
}

int sendSync(int socket, char *buf, struct sockaddr *dest_addr, int dest_len){
	buf[0] = (PTYPE_SYN >> 5) + XORFREQ;
	memset(&(buf[1]), 0,BUFFSIZE - 1);
	return sendto(socket,buf,BUFFSIZE,0,(struct sockaddr *)&dest_addr,dest_len);

}

int sendAck(int socket, char *buf, struct sockaddr *dest_addr, int dest_len, int seq_number){
	buf[0] = (PTYPE_ACK >> 5) + seq_number;
	memset(&(buf[1]), 0,BUFFSIZE - 1);
	return sendto(socket,buf,BUFFSIZE,0,(struct sockaddr *)&dest_addr,dest_len);

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
	hints.ai_protocol = 0;          /* Any protocol, as we define our own */
	hints.ai_canonname = NULL;
	hints.ai_addr = NULL;
	hints.ai_next = NULL;

	s = getaddrinfo(NULL, argv[1], &hints, &result);
	if (s != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
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
		fprintf(stderr, "Could not bind\n");
		exit(EXIT_FAILURE);
	}

	freeaddrinfo(result);           /* No longer needed */
	/* CONNECTED TO CLIENT - SYNC PROCESS BEGINS */
	if(recvfrom(sfd, buf, BUFFSIZE, 0,(struct sockaddr*) &peer_addr, &peer_addr_len)== -1){
		perror("Sync error on first receive");
	}

	if(readsync(buf)==-1){
		perror("Sync error on reading sync");
		exit(EXIT_FAILURE);
	}
	
	if(sendSync(sfd, buf, peer_addr, peer_addr_len) ==-1){
		perror("Sync error on first send");
		exit(EXIT_FAILURE);
	}
	

		
}









