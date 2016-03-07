
/*
 A simple client in the internet domain using TCP
 Usage: ./client hostname port (./client 192.168.0.151 10000)
 */
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>      // define structures like hostent
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>

#include "structs.h"

void error(char *msg)
{
    perror(msg);
    exit(0);
}

int main(int argc, char *argv[])
{
  /*
    int len = 200;
    char *buffer = malloc(sizeof(char)*len);
    */
  int len = 255;
  char buffer[256];

    if (argc < 3) {
       fprintf(stderr,"usage %s hostname port\n", argv[0]);
       exit(0);
    }
    
    socket_info_st *s = init_socket(atoi(argv[2]), argv[1], 0);
     

    printf("Please enter the hello message: ");
    memset(buffer,0, len);
    fgets(buffer,len,stdin);	//read message
    socket_send(s, buffer, strlen(buffer));
    

    memset(buffer,0,len);
    socket_recv(s, buffer, len);
    printf("I got this data: %s\n", buffer);

    memset(buffer,0, len);
    buffer[0] = 'a';
    buffer[1] = 'c';
    buffer[2] = 'k';
    socket_send(s, buffer, strlen(buffer));

    free_socket(s);

    return 0;
}
