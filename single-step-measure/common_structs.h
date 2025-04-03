#define PORT 8008
#define BUFFER_SIZE 100
#define MAXLINE 1000

#define MAX_STEP_AMOUNT 20
#define PAGE_TRACKS 1000
#define MAX_RUN_TIME 5 //Timeout in ms.

#define PINGPONG_LEN 3

typedef struct
{
    uint64_t gpa1;
    uint64_t gpa2;
} pagetrack_gpas_t;