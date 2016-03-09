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
#include <time.h>

#include "structs.h"

#define RECEIVER_DEBUG 1
#define RECEIVER_DEBUG_PACKET 0
#define RECEIVER_DEBUG_WRITE 0
#define RECEIVER_DEBUG_SLIDE 1
#define RECEIVER_DEBUG_INIT 0

void error(char *msg)
{
    perror(msg);
    exit(0);
}



typedef struct window_frame {
    int recv; //1 if this frame has data, 0 if it does not
    int sequ_no; //sequence number, between 0 and MAX_SEQ_NO - 1
    int size; //size of data
    char buffer[PACKET_SIZE]; //contains data
} window_frame_st;

typedef struct window {
  int idx;
  FILE *fd;
  window_frame_st frames[WINDOW_SIZE]; 
} window_st;

//////////////////////////////////////
//////////////////////////////////////

window_st *window_init(char *filename) {
    if (RECEIVER_DEBUG_INIT)
        printf("Intializing window struct\n");

    int i;
    window_st *w = malloc(sizeof(window_st)); 

    w->idx = 0;

    w->fd = fopen(filename, "w");

    for (i = 0; i < WINDOW_SIZE; i++) {
       memset(&(w->frames[i]), 0, sizeof(window_frame_st));
       w->frames[i].sequ_no = i * DATA_SIZE;
       if (RECEIVER_DEBUG_INIT)
           printf("Intializing sequ_no: %d\n", w->frames[i].sequ_no);
    }

    if (RECEIVER_DEBUG_INIT)
        printf("Finished initializing window struct\n");

    return w;
}




//if returns 0, then it could not find the frame with the matching sequ_no
//if returns 1, then it wrote the packet data to the correct frame
int window_write(window_st *w, int sequ_no, int data_len, char *buffer) {

    if (RECEIVER_DEBUG_WRITE)
        printf("ENTERING WINDOW_WRITE FUNCTION\n");

    int i; 
    window_frame_st *f;
    for (i = 0; i < WINDOW_SIZE; i++) {
        if (w->frames[(i + w->idx) % WINDOW_SIZE].sequ_no == sequ_no) {
            f = &(w->frames[(i + w->idx) % WINDOW_SIZE]);
            memcpy(f->buffer, buffer, sizeof(char) * data_len);
            f->size = data_len;
            f->recv = 1;
            if (RECEIVER_DEBUG_WRITE)
                printf("SET THE PACKET DATA IN AN APPROPRIATE WINDOW FRAME\n");
            return 1;
        }
    }
    if (RECEIVER_DEBUG_WRITE)
        printf("FAILED TO FIND APPROPRIATE FRAME\n");
    return 0;

}

//if returns 0, then it could not write another frame to file
//if returns 1, then it "moved the window forward by one", and wrote a frame to the file
int window_slide(window_st *w) {
    
    window_frame_st *f = &w->frames[w->idx];
    int temp = f->sequ_no;

    if (f->recv == 0)
        return 0;
    else {
        if (RECEIVER_DEBUG_SLIDE)
            printf("writing frame with sequence number: %d\n", temp);
        fwrite(&(f->buffer), sizeof(char), f->size, w->fd);
        memset(f, 0, sizeof(window_frame_st));
        f->sequ_no = temp + (WINDOW_SIZE * DATA_SIZE) % MAX_SEQ_NO;
        w->idx = (w->idx + 1) % WINDOW_SIZE;
        return 1;
    }
}


//Returns the sequence number of the closest unreceived packet
int window_update(window_st *w, int sequ_no, int data_len, char *buffer) {
    
    if (window_write(w, sequ_no, data_len, buffer) == 1) {
        while(window_slide(w) == 1);
    }

    return w->frames[w->idx].sequ_no;
}



void window_free(window_st *w) {
    fclose(w->fd);
    free(w);
}

char *find_data_start(char *buffer) {
    int i;
    for (i = 0; i < PACKET_SIZE; i++) {
        if (buffer[i] == '\n')
            return buffer + i + 1;
    }

    my_err("corrupted/incorrectly formatted packet header");
    return 0;

}

/////////////////////////////////////////////////////
/////////////////////////////////////////////////////

int main(int argc, char *argv[])
{
    srand(time(NULL));
    int len = 1000;
    char *buffer = malloc(sizeof(char)*len);

    char *data_buffer;

    char header_type[HEADER_SIZE];
    int sequ_no;
    int data_len;

    double p_corr = 0.0;
    double p_drop = 0.0;

    if (argc < 3) {
        fprintf(stderr,"usage %s hostname port\n", argv[0]);
        exit(0);
    }
    
    socket_info_st *s = init_socket(atoi(argv[2]), argv[1], 0);
     
    printf("Please enter the desired filename: ");
    memset(buffer,0, len);
    fgets(buffer,len,stdin);  //read message
    socket_send(s, buffer, strlen(buffer));
    
    //window_st *w = window_init(buffer);
    window_st *w = window_init("downloaded_file");

    while (1) {
        socket_recv(s, buffer, len);

        sscanf(buffer, "%s %d %d", header_type, &sequ_no, &data_len);

        if (RECEIVER_DEBUG) {
            printf("\n/////////////////////////\nDebugging Info:\n");
            printf("Header type: %s\n", header_type);
            printf("Sequence number: %d\n", sequ_no);
            printf("Data length: %d\n", data_len);
            printf("/////////////////////////\n");
        }

        if (RECEIVER_DEBUG_PACKET) 
            printf("%s", buffer);

        p_corr = (rand() % 10) / 10.0;
        p_drop = (rand() % 10) / 10.0;
        if (RECEIVER_DEBUG) {
          printf("corruped packet probability and threshold are: %f %f\n", p_corr, P_CORRUPT);
          printf("dropped packet probability and threshold:%f %f\n", p_drop, P_DROPPED);
        }


        if ((p_drop >= P_DROPPED) && (p_corr >= P_CORRUPT)) {
            sequ_no = w->frames[w->idx].sequ_no;
            printf("Sequence number: %d\n", sequ_no);
            data_buffer = find_data_start(buffer);
            window_update(w, sequ_no, data_len, data_buffer);
            //sequ_no = window_update(w, sequ_no, data_len, data_buffer);

            memset(buffer, 0, len);
            // julien left this here
            //sprintf(buffer, "ACK %d OK", (sequ_no - 900));
            sprintf(buffer, "ACK %d OK", sequ_no);
            socket_send(s, buffer, PACKET_SIZE);
            if (data_len != DATA_SIZE)
                break;
        }
        else if (p_corr < P_CORRUPT) {
            memset(buffer, 0, len);
            sprintf(buffer, "ACK %d CORRUPT", sequ_no);
            socket_send(s, buffer, PACKET_SIZE);
        }

    }

    free_socket(s);
    window_free(w);

    return 0;
}
