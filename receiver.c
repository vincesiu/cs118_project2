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

#define HOTFIX_FOR_JULIEN 0
//0 == ACK gives the received sequence number
//1 == ACK gives the earliest sequence number the receiver still needs

#define RECEIVER_DEBUG 0
#define RECEIVER_DEBUG_PACKET 0
#define RECEIVER_DEBUG_UPDATE 0
#define RECEIVER_DEBUG_SLIDE 0
#define RECEIVER_DEBUG_INIT 0

#define RECEIVER_PRINT 1

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
    int last_len_written; //gg too lazy to refactor this, basically if this is not 900, AND the window_clear function returns true, then I'm done
    window_frame_st frames[WINDOW_LEN]; 
} window_st;

//////////////////////////////////////
//////////////////////////////////////

window_st *window_init(char *filename) {
    if (RECEIVER_DEBUG_INIT)
        printf("Intializing window struct\n");

    int i;
    window_st *w = malloc(sizeof(window_st)); 

    w->idx = 0;

    w->fd = fopen(filename, "wb");

    w->last_len_written = DATA_SIZE;

    for (i = 0; i < WINDOW_LEN; i++) {
       memset(&(w->frames[i]), 0, sizeof(window_frame_st));
       w->frames[i].sequ_no = i * DATA_SIZE;
       if (RECEIVER_DEBUG_INIT)
           printf("Intializing sequ_no: %d\n", w->frames[i].sequ_no);
    }

    if (RECEIVER_DEBUG_INIT)
        printf("last len written initialized as: %d\n", w->last_len_written);

    if (RECEIVER_DEBUG_INIT)
        printf("Finished initializing window struct\n");

    return w;
}



//0 for not clear
//1 for clear
int window_clear(window_st *w) {
    int i;
    for (i = 0; i < WINDOW_LEN; i++) {
        if (w->frames[i].recv == 1)
            return 0;
    }
    return 1;
}

//if returns 0, then it could not find the frame with the matching sequ_no
//if returns 1, then it wrote the packet data to the correct frame
//This writes from packet to window
int window_update(window_st *w, int sequ_no, int data_len, char *buffer) {


    int i; 
    window_frame_st *f;
    for (i = 0; i < WINDOW_LEN; i++) {
        if (w->frames[(i + w->idx) % WINDOW_LEN].sequ_no == sequ_no) {
            f = &(w->frames[(i + w->idx) % WINDOW_LEN]);
            memcpy(f->buffer, buffer, sizeof(char) * data_len);

            if (RECEIVER_DEBUG_UPDATE) {
                printf("window_update: put packet in the window:\n");
                //printf("BUFFER---------------\n\n%s", buffer);
            }
            f->size = data_len;
            f->recv = 1;
            if (RECEIVER_DEBUG_UPDATE) {
                printf("f->size has value of: %d\n", f->size);
                printf("SET THE PACKET DATA IN AN APPROPRIATE WINDOW FRAME\n");
            }
            return 1;
        }
    }
    if (RECEIVER_DEBUG_UPDATE)
        printf("window_update: did not put the packet in the window\n");
    return 0;

}

//if returns 0, then it could not write another frame to file
//if returns 1, then it "moved the window forward by one", and wrote a frame to the file
//This writes from window to disk
int window_slide(window_st *w) {
    
    window_frame_st *f = &w->frames[w->idx];
    int temp = f->sequ_no;

    if (f->recv == 0)
        return 0;
    else {
        fwrite(&(f->buffer), sizeof(char), f->size, w->fd);
        if (w->last_len_written == DATA_SIZE)
            w->last_len_written = f->size;
        if (RECEIVER_DEBUG_SLIDE) {
            printf("writing frame with sequence number: %d\n", temp);
            //printf("last len written updated to: %d\n", w->last_len_written);
            printf("f->size is %d\n", w->last_len_written);
            //printf("current sequence number is %d\n", w->frames[w->idx].sequ_no);
            //printf("next sequence number is %d\n", w->frames[(w->idx + 1) % WINDOW_LEN].sequ_no);
        }
        memset(f, 0, sizeof(window_frame_st));
        f->sequ_no = (temp + (WINDOW_LEN * DATA_SIZE)) % MAX_SEQ_NO;
        w->idx = (w->idx + 1) % WINDOW_LEN;
        return 1;
    }
}


//Returns 1 if the given sequence number could be written
//else 0
int window_recv(window_st *w, int sequ_no, int data_len, char *buffer) {
    int temp = window_update(w, sequ_no, data_len, buffer);

    if (temp == 1) {
        while(window_slide(w) == 1);
    }

    return temp;
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
     
    printf("RECEIVER: Please enter the desired filename: ");
    memset(buffer,0, len);
    fgets(buffer,len,stdin);  //read message
    socket_send(s, buffer, strlen(buffer));
    
    //window_st *w = window_init(buffer);
    window_st *w = window_init("downloaded_file");
    
    socket_recv(s, buffer, len);
    sscanf(buffer, "%s", header_type);
    if (strcmp(header_type, "REJECT") == 0) {
        my_err("did not request valid file\n");
    }
    else if (strcmp(header_type, "ACCEPT") == 0) {
        printf("RECEIVER: received ACCEPT packet, initiating data transmission\n");
    }
    else {
        printf("RECEIVER: received corrupted ACCEPT packet, exiting\n");
        return 1;
    }

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

        if (RECEIVER_PRINT) {
            if (strcmp(header_type, "SEND") == 0)
                printf("RECEIVER: received DATA packet   %d\n", sequ_no);
            else if (strcmp(header_type, "RESEND") == 0)
                printf("RECEIVER: received RESEND packet %d\n", sequ_no);
            else {
                printf("RECEIVER: received corrupted packet\n");
                continue;
            }

        }


        p_corr = ((rand() % 1000) + 1)/ 1000.0;
        p_drop = ((rand() % 1000) + 1)/ 1000.0;

        if (RECEIVER_DEBUG) {
            printf("corruped packet probability and threshold are: %f %f\n", p_corr, P_CORRUPT);
            printf("dropped packet probability and threshold:%f %f\n", p_drop, P_DROPPED);
        }


        if ((p_drop > P_DROPPED) && (p_corr > P_CORRUPT)) {
            data_buffer = find_data_start(buffer);

            if (window_recv(w, sequ_no, data_len, data_buffer) == 1) {
                if (HOTFIX_FOR_JULIEN) {
                    sequ_no = w->frames[w->idx].sequ_no;
                }

                if (RECEIVER_DEBUG) {
                    printf("Buffer-----%s\n\n", buffer);
                }

                if (RECEIVER_PRINT) {
                    printf("RECEIVER: sending ACK packet     %d\n", sequ_no);
                }

                memset(buffer, 0, len);
                sprintf(buffer, "ACK %d OK", sequ_no);
                socket_send(s, buffer, PACKET_SIZE);


                if (RECEIVER_DEBUG) {
                    printf("Last length written: %d\n", w->last_len_written);
                }
                if ((w->last_len_written != DATA_SIZE) && (window_clear(w) == 1))
                    break;
            }
        }
        else if (p_corr <= P_CORRUPT) {
            if (RECEIVER_PRINT) {
                printf("RECEIVER: this packet is corrupted \n");
            }
            memset(buffer, 0, len);
            sprintf(buffer, "ACK %d CORRUPT", sequ_no);
            socket_send(s, buffer, PACKET_SIZE);
        }

    }

    free_socket(s);
    window_free(w);

    return 0;
}
