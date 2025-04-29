#define PORT 8008
#define MAXLINE 1000
#define BUFFER_SIZE 100


#define MAX_STEP_AMOUNT 200000
#define PAGE_TRACKS 500
#define MAX_RUN_TIME 200 //Timeout in ms.

#define MAX_ZERO_STEPS 100000

typedef struct
{
    uint64_t gpa1;
    uint64_t gpa2;
} pagetrack_gpas_t;