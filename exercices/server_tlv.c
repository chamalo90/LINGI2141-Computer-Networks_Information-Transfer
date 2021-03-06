#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
 
#define SERVER_PORT 62141
 
int main(int argc, char**argv)
{
 
 
	int listenfd;
	struct sockaddr_in6 servaddr;
 
	listenfd=socket(AF_INET6,SOCK_STREAM,0);
	if(listenfd == -1){
		perror("Socket creation");
	}
 
	servaddr.sin6_family = AF_INET6;
	servaddr.sin6_addr   = in6addr_any;
	servaddr.sin6_port=htons(62141);
	servaddr.sin6_scope_id=0;

	int bind_f = bind(listenfd,(struct sockaddr *)&servaddr,sizeof(servaddr));
	if(bind_f == -1){
		perror("Bind");
	}
	printf("Bind\n");


	int listen_f = listen(listenfd,5);
	if(listen_f == -1){
		perror("Listen");
	}
	printf("Listen\n");
 
	int client_sock;
	struct sockaddr_in claddr;
	int len = (sizeof(claddr));
	client_sock = accept(listenfd, (struct sockaddr*) &claddr, (socklen_t *) &len);
	if(client_sock == -1){
		perror("Accept");
	}
	printf("Accept\n");

	char buffer[65];
	printf("Write your message (64 char max):\n");
	scanf("%64s",buffer+1);
	fflush(stdin);
	unsigned int strsize = strlen(buffer+1);
	buffer[0]= 0x40 | strsize; 
	int sendfd = send(client_sock, buffer, strsize + sizeof(char),0);
	if(sendfd == -1){
		perror("Send");
	}
	char resp[4];
	int recvsize = recv(client_sock, resp, sizeof(int), 0);
	if(recvsize == -1){
		perror("Recv");
	}else if(recvsize != sizeof(int)){
		printf("Wrong size\n");
	}
	if(resp[3] == buffer[0]){
		resp[0] = 0x81;
		resp[1] = 0x01;
	}else{
		resp[0] = 0x81;
		resp[1] = 0x00;
	}
	sendfd = send(client_sock, resp, sizeof(char)*2,0);
	if(sendfd == -1){
		perror("Second recv");
	}
	
	close(client_sock);
	close(listenfd);
	return 0;
}
