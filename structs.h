#include <sys/time.h>
#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <strings.h>


#define PACKET_SIZE 1000
#define WINDOW_SIZE 5000
#define MAX_SEQ_NO  30000
#define TIMEOUT     3000   // in ms


#define DEBUG_RECEIVE 0

typedef struct frame_node {
    char* data; // header + data, cannot exceed PACKET_SIZE
    int len; // length of JUST DATA
    int seq_no; // in bytes, cannot exceed MAX_SEQ_NO
    int ack;  // 0 if ack not yet sent/received, 1 if ack sent/received
    int corrupt; // 0 if normal, 1 if corrupt
    int lost; // 0 if normal, 1 if lost
    struct timeval timeSent;    
} Frame;


typedef struct socket_info {
    int who; // 0: receiver, 1: sender
    int sockfd;
    struct sockaddr_in receiver;
    struct sockaddr_in sender;
    socklen_t len;
} socket_info_st;


void my_err(char *msg) {
    fprintf(stderr, "ERROR: %s\n", msg);
    exit(1);
}

//portno: which port?
//hostname: null if you're the sender, and its localhost or whatever if you're the receiver 
//who: are we initializing the sender or the receiver side?
//  0: reciever, this means we will setup the ports to initially connect to a server
//  1: sender, this means we will bind the ports
socket_info_st *init_socket(int portno, char *hostname, int who) {
    socket_info_st *ret = malloc(sizeof(socket_info_st));

    if((ret->sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        my_err("opening socket failed");

    ret->len = (socklen_t) sizeof(ret->receiver);
    
    ret->who = who;

    if (who == 1) {
        memset((char *) &ret->sender, 0, sizeof(ret->sender));
        ret->sender.sin_family = AF_INET;
        ret->sender.sin_addr.s_addr = INADDR_ANY;
        ret->sender.sin_port = htons(portno);

        if (bind(ret->sockfd, (struct sockaddr *) &ret->sender, ret->len) < 0)
            my_err("binding failed");
    }
    else if (who == 0) {
        struct hostent *server = gethostbyname(hostname);
        if (server == NULL)
            my_err("no such host");
        memset((char *) &ret->sender, 0, sizeof(ret->sender));
        ret->sender.sin_family = AF_INET;
        ret->sender.sin_port = htons(portno);
        bcopy((char *)server->h_addr, (char *)&ret->sender.sin_addr.s_addr, server->h_length);
        
    }
    else {
      my_err("Unknown second parameter passed to initialize_socket");
    }

    return ret;
}




//please remember to memset the buffer
void socket_recv(socket_info_st *s, char *buffer, int len) {
    if (DEBUG_RECEIVE)
        printf("start recv\n");
    if (s->who == 1) 
        recvfrom(s->sockfd, buffer, len, 0, (struct sockaddr *) &(s->receiver), &(s->len));
    else
        recvfrom(s->sockfd, buffer, len, 0, (struct sockaddr *) &(s->sender), &(s->len));
    if (DEBUG_RECEIVE)
        printf("end recv\n");
}





void socket_send(socket_info_st *s, char *buffer, int len) {
    if (s->who == 1) 
        sendto(s->sockfd, buffer, len, 0, (struct sockaddr *) &(s->receiver), s->len);
    else
        sendto(s->sockfd, buffer, len, 0, (struct sockaddr *) &(s->sender), s->len);
}

void free_socket(socket_info_st *s) {
    close(s->sockfd);
    free(s);
}
