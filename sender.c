/* A simple server in the internet domain using TCP
   The port number is passed as an argument 
   This version runs forever, forking off a separate 
   process for each connection
*/
#include <stdio.h>
#include <sys/types.h>   // definitions of a number of data types used in socket.h and netinet/in.h
#include <sys/socket.h>  // definitions of structures needed for sockets, e.g. sockaddr
#include <netinet/in.h>  // constants and structures needed for internet domain addresses, e.g. sockaddr_in
#include <stdlib.h>
#include <strings.h>
#include <sys/wait.h>	/* for the waitpid() system call */
#include <signal.h>	/* signal name macros, and the kill() prototype */
#include <string.h>
#include <unistd.h>


void error(char *msg)
{
    perror(msg);
    exit(1);
}

int main(int argc, char *argv[])
{
     int sockfd, portno;
     struct sockaddr_in serv_addr;

     if (argc < 2) {
         fprintf(stderr,"ERROR, no port provided\n");
         exit(1);
     }
     sockfd = socket(AF_INET, SOCK_DGRAM, 0);	//create socket
     if (sockfd < 0) 
        error("ERROR opening socket");
     memset((char *) &serv_addr, 0, sizeof(serv_addr));	//reset memory
     //fill in address info
     portno = atoi(argv[1]);
     serv_addr.sin_family = AF_INET;
     serv_addr.sin_addr.s_addr = INADDR_ANY;
     serv_addr.sin_port = htons(portno);
     
     if (bind(sockfd, (struct sockaddr *) &serv_addr,
              sizeof(serv_addr)) < 0) 
              error("ERROR on binding");
     
   	 char buffer[256];
   	 memset(buffer, 0, 256);	//reset memory
     struct sockaddr_in client;

     int client_len = sizeof(client);
     //recv(sockfd, buffer, sizeof(buffer), 0);
     recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&client, (socklen_t *) &client_len);
   	 printf("Here is the message: %s\n",buffer);

     memset(buffer, 0, 256);
     buffer[0] = 'm';

     sendto(sockfd, buffer, strlen(buffer) + 1, 0, (struct sockaddr *) &client, sizeof(client));
     close(sockfd);
         
     return 0; 
}

