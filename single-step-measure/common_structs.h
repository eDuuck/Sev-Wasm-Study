#define PORT 8008
#define BUFFER_SIZE 100
#define MAXLINE 1000

#define MAX_STEP_AMOUNT 40000
#define PAGE_TRACKS 500
#define MAX_RUN_TIME 20 //Timeout in ms.

#define MAX_ZERO_STEPS 1000

#define PINGPONG_LEN 1

typedef struct
{
    uint64_t gpa1;
    uint64_t gpa2;
} pagetrack_gpas_t;