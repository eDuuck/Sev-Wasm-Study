#define PINGPONG_LEN 2
#define BUFFER_SIZE 100
#define MAXLINE 1000
#define PORT 8008
#define PAGE_SIZE 4096
#define ASM_REP 100


enum callfunctions {
    PING,
    PINGPONG,
    ASM_ADD,
    ASM_MUL,
};