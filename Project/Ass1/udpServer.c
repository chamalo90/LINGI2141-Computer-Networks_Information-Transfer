#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

int main(int argc, char**argv)
{
   int sockfd,n;
   struct sockaddr_in6 servaddr,cliaddr;
   socklen_t len;
   char mesg[1000];

   sockfd=socket(AF_INET6,SOCK_DGRAM,0);

   bzero(&servaddr,sizeof(servaddr));
   servaddr.sin6_family = AF_INET6;
   servaddr.sin6_addr= in6addr_any;
   servaddr.sin6_port=htons(32000);
   servaddr.sin6_scope_id=0;
   bind(sockfd,(struct sockaddr_in6*)&servaddr,sizeof(servaddr));
   
	
	while(1){
      len = sizeof(cliaddr);
      n = recvfrom(sockfd,mesg,1000,0,(struct sockaddr *)&cliaddr,&len);
      sendto(sockfd,mesg,n,0,(struct sockaddr *)&cliaddr,len);
      printf("-------------------------------------------------------\n");
      mesg[n] = 0;
      printf("Received the following:\n");
      printf("%s",mesg);
      printf("-------------------------------------------------------\n");
	}
}
