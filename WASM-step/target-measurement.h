#define CAND_DIST_THRESHOLD 5
#define CAND_SET_SIZE 500
//#define TRACK_MODE KVM_PAGE_TRACK_WRITE
#define MAX_TRACK_TIME 10
#define LOWER_MEM_BOUND 0x00100000
#define UPPER_MEM_BOUND 0x5b03e000

#define PF_LIMIT 100
#define MULTISTEP_LIMIT 10
#define FILENAME_CHAR_LIM 100
#define FLUSH_LIM 1000000
//#define BUF_SIZE (128 * 1024 * 1024) // 128 MB buffer size (8 writes for 1GB)
#define PART_SIZE_LIMIT (5ULL * 1024ULL * 1024ULL * 1024ULL) // 5 GB
#define MAX_PATH_LEN 512

// --- Writer thread configuration ---
#define WRITE_QUEUE_SIZE 8192   // number of queued lines
#define FLUSH_INTERVAL_SEC 3    // flush to disk every N seconds
#define BUF_SIZE (1 << 20)      // 1 MB internal buffer for chunk copies

typedef struct
{
    bool set;
    uint16_t occurances;
    uint16_t cand_dist;
    uint64_t gpa1;
    uint64_t gpa2;
}page_tup_candidate;

typedef struct {
    char *lines[WRITE_QUEUE_SIZE];
    atomic_size_t head;
    atomic_size_t tail;
    atomic_bool done;
    pthread_mutex_t mtx;
    pthread_cond_t cv_nonempty;
    pthread_cond_t cv_nonfull;
} write_queue_t;