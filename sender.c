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

#include "structs.h"

void err(char *msg)
{
    fprintf(stderr, "ERROR: %s\n", msg);
    exit(1);
}

int main(int argc, char *argv[])
{
   	 char buffer[256];
    // char *buffer = malloc(sizeof(char) * 256);

     if (argc < 2) 
         err("no port provided");

     socket_info_st *s = init_socket(atoi(argv[1]), 0, 1);
     
   	 memset(buffer, 0, 256);	//reset memory
     buffer[0] = 'a';

     socket_recv(s, buffer);
   	 printf("Here is the message: %s\n",buffer);

     memset(buffer, 0, 256);
     buffer[0] = 'm';

     socket_send(s, buffer);
     free_socket(s); 
         
     return 0; 
}

