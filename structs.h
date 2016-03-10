#include <sys/time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdio.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <strings.h>

#define PACKET_SIZE 1000
#define HEADER_SIZE 100
#define DATA_SIZE   900 

#define WINDOW_SIZE 5000
#define WINDOW_LEN (WINDOW_SIZE / PACKET_SIZE)
#define MAX_SEQ_NO  30000
#define PACKET_LOSS_TIMEOUT 2 // in seconds

#define ENABLE_SOCKET_TIMEOUT 1
#define TIMEOUT_S   0 //timeout in seconds
#define TIMEOUT_US  500000 //timeout in microseconds, 100000 is 100 milliseconds

#define P_CORRUPT 0.0 //percentage of packets which will arrive corrupted, from 0 to 1
#define P_DROPPED 0.0 //percentage of packets which will arrive dropped, from 0 to 1
//Note that if you actually want all the packets to be corrupted, you need to set P_CORRUPT TO 1.1


#define DEBUG_RECEIVE 0

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
    struct timeval tv;
    tv.tv_sec = TIMEOUT_S;
    tv.tv_usec = TIMEOUT_US;

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

        if (ENABLE_SOCKET_TIMEOUT) {
            if(setsockopt(ret->sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
                my_err("could not set timeout value for sender socket");
        }

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

void socket_recv(socket_info_st *s, char *buffer, int len) {

    memset(buffer, 0, len);

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

//////////////////
//////////////////

// structure to hold the data window for the sender
// implemented as a linked list

typedef struct frame_node_sender {
    char data[PACKET_SIZE - 100]; // header + data, cannot exceed PACKET_SIZE
    int seq_no; // in bytes, cannot exceed MAX_SEQ_NO
    int len;
    int sent;
    int ack;  // 0 if ack not yet received, 1 if ack received
    int corrupt;
    int lost;
    struct frame_node_sender * next;
} Frame;

//////////////////
//////////////////

void print_window(Frame* window)
{
    Frame* it = window;
    int count = 1;
    while (it != NULL)
    {
        printf("F%i %i %i s=%i a=%i\n", count, it->seq_no, it->len, it->sent, it->ack);
        it = it->next;
        count++;
    }
}

Frame* update_window(Frame* window, FILE* fd, int filelen, int left)
{
    int replacecount;
    int framesize = PACKET_SIZE - 100;
    Frame* it = window;
    Frame* new_window = window;
    int i;
    int start = 0; // Start is computed below

    // set replacecount, start, it values
    if (window == NULL)
        replacecount = WINDOW_LEN;
    else
        replacecount = 0;

    // deal with ACK'd frames, FREE them
    // will not run if window is NULL (first run)
    while (new_window != NULL)
    {
        if (new_window->ack == 1)
        {
            it = new_window;
            new_window = it->next;
            start = it->seq_no + framesize;
            free(it);
            replacecount++;
        }
        else
            break;
    }

    // also we need this thing to end if file is complete
    if (left <= 0)
        return new_window;

    // if we need to completely redraw the window
    // either on first run, or if all sent frames in window were ACK'd
    if (new_window == NULL)
    {
        new_window = calloc(1, sizeof(Frame));
        it = new_window;
    }
    else if (replacecount > 0)
    {
        it = new_window;
        while (it->next != NULL)
            it = it->next;
        start = it->seq_no + framesize;
        it->next = calloc(1, sizeof(Frame));
        it = it->next;
    }
    // if there is nothing in the window to replace
    else
    {
        return window;
    }
    
    // allocate and fill new frames
    for (i = 0; i < replacecount; i++)
    {
        size_t len;
        it->seq_no = (start + i*framesize) % MAX_SEQ_NO;
        left -= framesize;
        if (left >= 0)
        {
            len = framesize;
            fread(it->data, sizeof(it->data), 1, fd);
        }
        else
        {
            len = (filelen % MAX_SEQ_NO) - it->seq_no;
            fread(it->data, len, 1, fd);
        }

        it->len = len;

        if (len < framesize)
        {
            it->next = NULL;
            it->len = len;
            break;
        }
        if (i < replacecount - 1)
        {
            it->next = calloc(1, sizeof(Frame));
            it = it->next;
        }
        
    }

    return new_window;
}

void free_window(Frame* window)
{
    if (window == NULL)
        return;
    else
    {
        free_window(window->next);
        free(window);
    }
}

// sends everything the window but not already ack'd
int send_window(socket_info_st *s, Frame* window)
{
    int ret = 0;
    char packet[PACKET_SIZE];
    while (window != NULL)
    {
        memset(packet, 0, PACKET_SIZE);
        if (window->ack == 0 && window->sent == 0) {
            // header: SEND <seq_no> <len>
            sprintf(packet, "SEND %d %d\n", window->seq_no, window->len);
            printf("SEND %d %d\n", window->seq_no, window->len);
            strcat(packet, window->data);
            socket_send(s, packet, PACKET_SIZE);
            ret += window->len;
            window->sent = 1;
            
        }
        else if (window->lost == 1) {
            sprintf(packet, "RESEND %d %d\n", window->seq_no, window->len);
            printf("RESEND %d %d due to packet loss.\n", window->seq_no, window->len);
            strcat(packet, window->data);
            socket_send(s, packet, PACKET_SIZE);
            window->lost = 0;
        }
        else if (window->corrupt == 1) {
            sprintf(packet, "RESEND %d %d\n", window->seq_no, window->len);
            printf("RESEND %d %d due to corrupt packet.\n", window->seq_no, window->len);
            strcat(packet, window->data);
            socket_send(s, packet, PACKET_SIZE);
            window->corrupt = 0;
        }
        window = window->next;
    }
    return ret;
}

int check_acks(Frame* window)
{
    if (window == NULL)
        return 1;
    else
    {
        if (window->ack == 1 && check_acks(window->next) == 1)
            return 1;
        else
            return 0;
    }
}

void process_ack(Frame* window, int seqno, int ok) {
    if (window == NULL)
        return;
    else if (window->seq_no == seqno)
    {
        if (ok == 1)
        {
            window->ack = 1;
            printf("ACK %d ok.\n", seqno);
        }
        else
        {
            window->corrupt = 1;
            printf("ACK %d corrupt.\n", seqno);
        }
        return;
    }
    else
        process_ack(window->next, seqno, ok);
}

int window_all_done(Frame* w)
{
    if (w == NULL)
        return 1;
    else
    {
        if ((w->ack == 1 || w->corrupt == 1) && window_all_done(w->next) == 1)
            return 1;
        else
            return 0;
    }
}
