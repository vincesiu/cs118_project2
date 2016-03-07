#include <sys/time.h>

typedef struct frame_node {
    char* data;
    int id;
    int ack;
    struct timeval timeSent;    
} Frame;
