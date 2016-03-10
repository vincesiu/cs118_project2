/*  A simple server in the internet domain using UDP
    The port number is passed as an argument */

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

int send_file(socket_info_st *s, FILE* fd)
{
    int len;
    int left;
    Frame* window = NULL;
    Frame* it;
    char buffer[PACKET_SIZE];
    struct timeval initial;
    struct timeval final;

    fseek(fd, 0L, SEEK_END);
    len = ftell(fd);
    fseek(fd, 0L, SEEK_SET);
    left = len;

    printf("frames: %d  len: %d\n", WINDOW_LEN, len);
    
    while (1)
    {
        if (window == NULL && left <= 0)
            break;
        
        window = update_window(window, fd, len, left);
        // print_window(window);
        left -= send_window(s, window);

        // get timeout ready for ACK processing
        gettimeofday(&initial, NULL);
        initial.tv_sec += PACKET_LOSS_TIMEOUT;

        // this loop processes ACK's, looks for corrupt and lost packets
        while (window != NULL && window->ack == 0 && window->corrupt == 0) 
        {
            int ack_seqno;
            char ack_status[20];
            socket_recv(s, buffer, PACKET_SIZE);
            if (buffer[0] != 0) 
            {
                sscanf(buffer, "ACK %d %s", &ack_seqno, ack_status);
                if (strcmp(ack_status, "OK") == 0)
                    process_ack(window, ack_seqno, 1);
                else
                    process_ack(window, ack_seqno, 0);
                // TODO: code to handle corrupt
            }
            memset(buffer, 0, PACKET_SIZE);

            // no reason to keep looping if all frames in window are ACK'd
            if (window_all_done(window))
                break;

            // end loop if timeout complete
            gettimeofday(&final, NULL);
            // printf("%d %d\n", initial.tv_sec, final.tv_sec);
            if (final.tv_sec >= initial.tv_sec)
            {
                it = window;
                while (it != NULL)
                {
                    if (it->corrupt == 0 && it->ack == 0)
                        it->lost = 1;
                    it = it->next;
                }
                break;
            }
        }
    }

    free_window(window);
    if (feof(fd))
        return 1;
    else
        return 0;
}

int main(int argc, char *argv[])
{
    //int len = 200;
     //char *buffer = malloc(sizeof(char) * (len + 1));
    int len = 255;
    char buffer[PACKET_SIZE];
    memset(buffer, 0, PACKET_SIZE);
    char filename[256];
    FILE* fp = NULL;

    if (argc < 2) 
    err("no port provided");

    socket_info_st *s = init_socket(atoi(argv[1]), 0, 1);

    while (fp == NULL)
    {
        socket_recv(s, buffer, len);
        sscanf(buffer, "%s", filename);
        fp = fopen(filename,"r");
        if (fp == NULL) {
            strcpy(buffer, "REJECT");
            socket_send(s, buffer, strlen(buffer));
            memset(buffer, 0, PACKET_SIZE);
        }
    }
    strcpy(buffer, "ACCEPT");
    socket_send(s, buffer, PACKET_SIZE);
    
    printf("Sending file %s\n", filename);

    send_file(s, fp);
    
    free_socket(s); 
       
    return 0; 
}

