#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <stdint.h>
#include <string.h>

#define PAGE_SIZE 4096

// Sync structures
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t start_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t stop_cond = PTHREAD_COND_INITIALIZER;
int start_flag = 0;
int stop_flag = 0;

// Allocate memory pages
void *allocate_pages(size_t size, int offset) {
    void *mem = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, offset);
    if (mem == MAP_FAILED) {
        perror("mmap failed");
        exit(EXIT_FAILURE);
    }
    return mem;
}

// Get physical address from /proc/self/pagemap
uint64_t get_phys_addr(void *vaddr) {
    int fd = open("/proc/self/pagemap", O_RDONLY);
    if (fd < 0) {
        perror("Failed to open pagemap");
        exit(EXIT_FAILURE);
    }

    uint64_t entry;
    off_t offset = ((uintptr_t)vaddr / PAGE_SIZE) * sizeof(entry);
    pread(fd, &entry, sizeof(entry), offset);
    close(fd);

    return (entry & 0x7FFFFFFFFFFFFF) * PAGE_SIZE; // Extract PFN & shift
}

// Worker thread function
void *ping_ponger_thread(void *arg) {
    void *page1 = allocate_pages(PAGE_SIZE);
    void *page2 = allocate_pages(PAGE_SIZE);
    uint64_t gpa1 = get_phys_addr(page1);
    uint64_t gpa2 = get_phys_addr(page2);
    
    printf("Worker: Page1 GPA = 0x%lx, Page2 GPA = 0x%lx\n", gpa1, gpa2);

    pthread_mutex_lock(&mutex);
    while (!start_flag) {
        pthread_cond_wait(&start_cond, &mutex);
    }
    pthread_mutex_unlock(&mutex);

    printf("Worker: Start signal received, beginning accesses...\n");

    int iter = 0;
    while (1) {
        pthread_mutex_lock(&mutex);
        if (stop_flag) {
            pthread_mutex_unlock(&mutex);
            break;
        }
        pthread_mutex_unlock(&mutex);
        
        // Memory access pattern (READ / WRITE / EXEC)
        *((volatile int *)page1) = 42;
        *((volatile int *)page2) = 42;
        
        iter++;
        if (iter >= 6) usleep(100000);
    }

    printf("Worker: Stop signal received, exiting...\n");
    return NULL;
}

int main() {
    pthread_t worker;
    pthread_create(&worker, NULL, ping_ponger_thread, NULL);

    sleep(1); // Simulate delay before starting
    pthread_mutex_lock(&mutex);
    start_flag = 1;
    pthread_cond_signal(&start_cond);
    pthread_mutex_unlock(&mutex);

    sleep(2); // Let it run for a bit
    pthread_mutex_lock(&mutex);
    stop_flag = 1;
    pthread_mutex_unlock(&mutex);
    
    pthread_join(worker, NULL);
    return 0;
}
