#define CAND_DIST_THRESHOLD 5
#define CAND_SET_SIZE 500
//#define TRACK_MODE KVM_PAGE_TRACK_WRITE
#define MAX_TRACK_TIME 10
#define LOWER_MEM_BOUND 0x00100000
#define UPPER_MEM_BOUND 0x5b03e000

typedef struct
{
    bool set;
    uint16_t occurances;
    uint16_t cand_dist;
    uint64_t gpa1;
    uint64_t gpa2;
}page_tup_candidate;