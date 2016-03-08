/*  A simple client in the internet domain using UDP
    Usage: ./client hostname port (./client 192.168.0.151 10000) */

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
    int len = 1000;
    char *buffer = malloc(sizeof(char)*len);

    if (argc < 3) {
        fprintf(stderr,"usage %s hostname port\n", argv[0]);
        exit(0);
    }
    
    socket_info_st *s = init_socket(atoi(argv[2]), argv[1], 0);
     
    printf("Please enter the desired filename: ");
    memset(buffer,0, len);
    fgets(buffer,len,stdin);  //read message
    socket_send(s, buffer, strlen(buffer));
    
    while (1) {
        socket_recv(s, buffer, len);
        printf("%s\n", buffer);
        memset(buffer, 0, len);
        buffer[0] = 'A';
        buffer[1] = 'C';
        buffer[2] = 'K';
        socket_send(s, buffer, 100);
    }

    free_socket(s);

    return 0;
}

typedef struct window {

} window_st;
