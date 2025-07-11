
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include <unistd.h>

#include <fcntl.h>

#include "vm-constants.h"

// Allocate memory pages
void *allocate_pages(size_t size, int offset)
{
    void *mem = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, offset);
    memset(mem, 0, PAGE_SIZE);
    if (mem == MAP_FAILED)
    {
        perror("mmap failed");
        exit(EXIT_FAILURE);
    }
    return mem;
}

// Get physical address from /proc/self/pagemap
uint64_t get_phys_addr(void *vaddr)
{
    int fd = open("/proc/self/pagemap", O_RDONLY);
    if (fd < 0)
    {
        perror("Failed to open pagemap");
        exit(EXIT_FAILURE);
    }

    uint64_t entry;
    off_t offset = ((uintptr_t)vaddr / PAGE_SIZE) * sizeof(entry);
    pread(fd, &entry, sizeof(entry), offset);
    close(fd);

    return (entry & 0x7FFFFFFFFFFFFF) * PAGE_SIZE; // Extract PFN & shift
}

int global_var = 1337;

int main()
{
    void *heap_ptr = malloc(1);

    void *page1 = allocate_pages(PAGE_SIZE, 0);
    void *page2 = allocate_pages(PAGE_SIZE, 0);
    uint64_t gpa1 = get_phys_addr(page1);
    uint64_t gpa2 = get_phys_addr(page2);

    int stack_var = 1337;

    printf("PID:            %d\n", getpid());
    printf("Code address:   %p\n", (void *)main);
    printf("Global address: %p\n", (void *)&global_var);
    printf("Stack address:  %p\n", (void *)&stack_var);
    printf("Heap address:   %p\n", heap_ptr);
    printf("Page1 GPA = 0x%lx, Page2 GPA = 0x%lx\n", gpa1, gpa2);

    printf("Proceeding to pingpong!\n\n")
    for (int i = 0; i < PINGPONG_LEN; i++)
    {
        *((volatile int *)page1) = i;
        *((volatile int *)page2) = i;
        printf("Pingpong %d done!\n",i);
    }
    return 1;
}