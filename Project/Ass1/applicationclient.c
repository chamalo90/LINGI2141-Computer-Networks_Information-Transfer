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

int sendMsg(int socket, char* buf, struct sockaddr *dest_addr, int dest_len, int type, int seq_number, int window){
	if(type == PTYPE_SYN){
		memset(&(buf[0]), 0,BUFFSIZE);
		buf[0] = (PTYPE_SYN >> 5);
		return sendto(socket,buf,BUFFSIZE,0,(struct sockaddr *)&dest_addr,dest_len);	
	}else if(type == PTYPE_ACK){
		buf[0] = (PTYPE_ACK >> 5);
	}
}

int main(int argc, char *argv[])
{
	struct addrinfo hints;
	struct addrinfo *result, *rp;
	int sfd, s, j;
	struct sockaddr* peer_addr;
	socklen_t peer_addr_len;
	ssize_t nread;
	char buf[BUFFSIZE];

	if (argc < 3) {
		fprintf(stderr, "Usage: %s host port\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	/* Obtain address(es) matching host/port */

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET6;    /* Allow IPv6 */
	hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
	hints.ai_flags = 0;
	hints.ai_protocol = 0;   /* Any protocol */

	s = getaddrinfo(argv[1], argv[2], &hints, &result);
	if (s != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
		exit(EXIT_FAILURE);
	}

	  /* getaddrinfo() returns a list of address structures.
	Try each address until we successfully connect(2).
	If socket(2) (or connect(2)) fails, we (close the socket
	and) try the next address. */

	for (rp = result; rp != NULL; rp = rp->ai_next) {
		sfd = socket(rp->ai_family, rp->ai_socktype,
		rp->ai_protocol);
 		if (sfd == -1)
			 continue;

		if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1)
			 break;/* Success */

		close(sfd);
	}

	if (rp == NULL) { /* No address succeeded */
		fprintf(stderr, "Could not connect\n");
		exit(EXIT_FAILURE);
	}
	freeaddrinfo(result);/* No longer needed */
	printf("connected\n");
	if(sendMsg(sfd, buf, peer_addr, peer_addr_len, PTYPE_SYN, 0, 0) ==-1){
		perror("Sync error on first send");
		exit(EXIT_FAILURE);
	}

}

