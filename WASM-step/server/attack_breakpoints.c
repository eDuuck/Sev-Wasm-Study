#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdbool.h>  // Optional, only if you want to use `true`/`false` in returns
#include "attack_breakpoints.h"

void *attack_page1;
void *attack_page2;
uint64_t phys1;
uint64_t phys2;

#define PAGE_SIZE 4096
static uint64_t  virt_to_phys(void *virt_addr)
{
    uint64_t virt_pn = (uint64_t)virt_addr / PAGE_SIZE;
    uint64_t offset  = virt_pn * sizeof(uint64_t);
    uint64_t entry   = 0;

    int fd = open("/proc/self/pagemap", O_RDONLY);
    if (fd < 0) {
        perror("Failed to open pagemap file");
        return 0;
    }
    if(lseek(fd, offset, SEEK_SET) < 0) {
        perror("Failed to seek in pagemap file");
        close(fd);
        return 0;
    }
    if(read(fd, &entry, sizeof(uint64_t)) != sizeof(uint64_t)) {
        perror("Failed to read from pagemap file");
        close(fd);
        return 0;
    }
    close(fd);

    if (!(entry & (1ULL << 63))) {
        perror("Page not present in memory");
        return 0;
    }

    uint64_t frame = entry & ((1ULL << 55) - 1);
    return (frame * PAGE_SIZE) + ((uint64_t)virt_addr & (PAGE_SIZE - 1));
}

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

void start_measurement_breakpoint()
{
    attack_page1 = allocate_pages(PAGE_SIZE, 0);
    attack_page2 = allocate_pages(PAGE_SIZE, 0);
    phys1 = virt_to_phys(attack_page1);
    phys2 = virt_to_phys(attack_page2);
    if (phys1 == 0 || phys2 == 0)
    {
        printf("Failed to allocate attack page\n");
        printf("Maybe you didn't run as sudo?\n");
        exit(EXIT_FAILURE);
    }
    printf("Virtual_1: %p → Physical_1: 0x%lx\n", attack_page1, phys1);
    printf("Virtual_2: %p → Physical_2: 0x%lx\n", attack_page2, phys2);
    printf("0x%lx-0x%lx\n", phys1/0x1000, phys2/0x1000);
    printf("Waiting for user input to start measurement...\n");
    *(volatile int *)attack_page1 = getchar(); // This is the breakpoint to start measurement
    *(volatile int *)attack_page2 = *(volatile int *)attack_page1 * 2;
}

void stop_measurement_breakpoint()
{
    *(volatile int*) attack_page1 = 0xdeadbeef; //This is the breakpoint to start measurement
    *(volatile int*) attack_page2 = *(volatile int*) attack_page1 * 2;
    printf("Measurement stopped.\n");
}