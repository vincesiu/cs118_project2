#include <sys/time.h>
#define PACKET_SIZE 1000
#define WINDOW_SIZE 5000
#define MAX_SEQ_NO  30000
#define TIMEOUT     3000   // in ms

typedef struct frame_node {
    char* data; // header + data, cannot exceed PACKET_SIZE
    int len; // length of JUST DATA
    int seq_no; // in bytes, cannot exceed MAX_SEQ_NO
    int ack;  // 0 if ack not yet sent/received, 1 if ack sent/received
    int corrupt; // 0 if normal, 1 if corrupt
    int lost; // 0 if normal, 1 if lost
    struct timeval timeSent;    
} Frame;
