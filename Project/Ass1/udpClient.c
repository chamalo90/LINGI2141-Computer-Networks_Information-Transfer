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
   char sendline[1000];
   char recvline[1000];



   sockfd=socket(AF_INET6,SOCK_DGRAM,0);

   bzero(&servaddr,sizeof(servaddr));
   servaddr.sin6_family = AF_INET6;
   servaddr.sin6_addr=htons(argv[1]);
   servaddr.sin6_port=htons(32000);
   servaddr.sin6_scope_id=0;

   while (fgets(sendline, 10000,stdin) != NULL)
   {
      sendto(sockfd,sendline,strlen(sendline),0,
             (struct sockaddr *)&servaddr,sizeof(servaddr));
      n=recvfrom(sockfd,recvline,10000,0,NULL,NULL);
      recvline[n]=0;
      fputs(recvline,stdout);
   }
}
