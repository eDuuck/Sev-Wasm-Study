
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include <unistd.h>

#include <fcntl.h>

#include <sys/socket.h>
#include <sys/mman.h>
#include <sys/types.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include "vm_server.h"

extern uint64_t asm_short();
extern void asm_add();
extern void asm_lar();
extern void asm_mul_2();
extern void asm_mul_1997();
extern void asm_mul_32bit();
extern void asm_mul_64bit();
extern void asm_rdrand();
extern void asm_nop();
extern void asm_lsl();

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
    char buffer[BUFFER_SIZE] = {0};
    int listen_fd, mess_len;
    struct sockaddr_in server_address, client_address;
    socklen_t addrlen = sizeof(server_address);

    memset(&server_address, 0, addrlen);

    // Socket() sets up a socket described by listen_fd, AD_INEe_T => IPv4, SOCK_DGRAM => UDP. (-1 on failure)
    listen_fd = socket(AF_INET, SOCK_DGRAM, 0);

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(PORT); // htons() switches from little-endian to big-endian used in network.

    bind(listen_fd, (struct sockaddr *)&server_address, sizeof(server_address));
    volatile int a;

    void *heap_ptr = malloc(1);

    void *page1 = allocate_pages(PAGE_SIZE, 0);
    void *page2 = allocate_pages(PAGE_SIZE, 0);
    uint64_t gpa1 = get_phys_addr(page1);
    uint64_t gpa2 = get_phys_addr(page2);

    int stack_var = 1337;

    printf("Version 15, Pingpong %d now. Added asm_lsl.\nRepeating ASM function %d times.\n", PINGPONG_LEN,ASM_REP);
    printf("PID:            %d\n", getpid());
    printf("Code address:   %p\n", (void*)main);
    printf("Global address: %p\n", (void*)&global_var);
    printf("Stack address:  %p\n", (void*)&stack_var);
    printf("Heap address:   %p\n", heap_ptr);
    printf("Worker: Page1 GPA = 0x%lx, Page2 GPA = 0x%lx\n", gpa1, gpa2);

    printf("Listening to port %d...\n\n", PORT);
    while (1)
    {
        mess_len = recvfrom(listen_fd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_address, &addrlen);
        buffer[mess_len] = '\0'; // Mark end of string.
        printf("Got message: \"%s\"\n", buffer);
        if (strcmp(buffer, "ping") == 0)
        {
            printf("Got a ping request.\n\n");
            sendto(listen_fd, "pong", 10, 0, (struct sockaddr *)&client_address, addrlen);
        }
        else if (strcmp(buffer, "pingpong") == 0)
        {
            printf("Got a pingpong request. Proceeding...\n\n");
            for (int i = 0; i < PINGPONG_LEN; i++)
            {
                *((volatile int *)page1) = 42;
                *((volatile int *)page2) = 42;
            }
            printf("Done! Informing client.\n");
            sendto(listen_fd, "done", 10, 0, (struct sockaddr *)&client_address, addrlen);
        }
        else if (strcmp(buffer, "pingpong add") == 0)
        {
            printf("Got a ping_add request. Proceeding.\n\n");
            for (int i = 0; i < PINGPONG_LEN; i++)
            {
                *((volatile int *)page1) = i;
                *((volatile int *)page2) = i;
            }
            for (int i = 0; i < 10; i++)
            {
                a += 5;
            }
            for (int i = 0; i < PINGPONG_LEN; i++)
            {
                *((volatile int *)page1) = i;
                *((volatile int *)page2) = i;
            }
            printf("Finished ping_add request. Proceeding.\n\n");
            sendto(listen_fd, "done", 10, 0, (struct sockaddr *)&client_address, addrlen);
        }
        else if (strcmp(buffer, "asm_add") == 0)
        {
            printf("Got a asm_add request. Proceeding.\n\n");
            for (int i = 0; i < PINGPONG_LEN; i++)
            {
                *((volatile int *)page1) = i;
                *((volatile int *)page2) = i;
            }
            for (int i = 0; i < ASM_REP; i++)
                asm_add();
            for (int i = 0; i < PINGPONG_LEN; i++)
            {
                *((volatile int *)page1) = i;
                *((volatile int *)page2) = i;
            }
            printf("Finished asm_add request. Proceeding.\n\n");
            sendto(listen_fd, "done", 10, 0, (struct sockaddr *)&client_address, addrlen);
        }
        else if (strcmp(buffer, "asm_mul_2") == 0)
        {
            printf("Got a asm_mul_2 request. Proceeding.\n\n");
            for (int i = 0; i < PINGPONG_LEN; i++)
            {
                *((volatile int *)page1) = i;
                *((volatile int *)page2) = i;
            }
            for (int i = 0; i < ASM_REP; i++)
                asm_mul_2();
            for (int i = 0; i < PINGPONG_LEN; i++)
            {
                *((volatile int *)page1) = i;
                *((volatile int *)page2) = i;
            }
            printf("Finished asm_mul_2 request. Proceeding.\n\n");
            sendto(listen_fd, "done", 10, 0, (struct sockaddr *)&client_address, addrlen);
        }
        else if (strcmp(buffer, "asm_mul_1997") == 0)
        {
            printf("Got a asm_mul_1997 request. Proceeding.\n\n");
            for (int i = 0; i < PINGPONG_LEN; i++)
            {
                *((volatile int *)page1) = i;
                *((volatile int *)page2) = i;
            }
            for (int i = 0; i < ASM_REP; i++)
                asm_mul_1997();
            for (int i = 0; i < PINGPONG_LEN; i++)
            {
                *((volatile int *)page1) = i;
                *((volatile int *)page2) = i;
            }
            printf("Finished asm_mul_1997 request. Proceeding.\n\n");
            sendto(listen_fd, "done", 10, 0, (struct sockaddr *)&client_address, addrlen);
        }
        else if (strcmp(buffer, "asm_mul_32bit") == 0)
        {
            printf("Got a asm_mul_32bit request. Proceeding.\n\n");
            for (int i = 0; i < PINGPONG_LEN; i++)
            {
                *((volatile int *)page1) = i;
                *((volatile int *)page2) = i;
            }
            for (int i = 0; i < ASM_REP; i++)
                asm_mul_32bit();
            for (int i = 0; i < PINGPONG_LEN; i++)
            {
                *((volatile int *)page1) = i;
                *((volatile int *)page2) = i;
            }
            printf("Finished asm_mul_32bit request. Proceeding.\n\n");
            sendto(listen_fd, "done", 10, 0, (struct sockaddr *)&client_address, addrlen);
        }
        else if (strcmp(buffer, "asm_mul_64bit") == 0)
        {
            printf("Got a asm_mul_64bit request. Proceeding.\n\n");
            for (int i = 0; i < PINGPONG_LEN; i++)
            {
                *((volatile int *)page1) = i;
                *((volatile int *)page2) = i;
            }
            for (int i = 0; i < ASM_REP; i++)
                asm_mul_64bit();
            for (int i = 0; i < PINGPONG_LEN; i++)
            {
                *((volatile int *)page1) = i;
                *((volatile int *)page2) = i;
            }
            printf("Finished asm_mul_64bit request. Proceeding.\n\n");
            sendto(listen_fd, "done", 10, 0, (struct sockaddr *)&client_address, addrlen);
        }
        else if (strcmp(buffer, "asm_lar") == 0)
        {
            printf("Got a asm_lar request. Proceeding.\n\n");
            for (int i = 0; i < PINGPONG_LEN; i++)
            {
                *((volatile int *)page1) = i;
                *((volatile int *)page2) = i;
            }
            for (int i = 0; i < ASM_REP; i++)
                asm_lar();
            for (int i = 0; i < PINGPONG_LEN; i++)
            {
                *((volatile int *)page1) = i;
                *((volatile int *)page2) = i;
            }
            printf("Finished asm_lar request. Proceeding.\n\n");
            sendto(listen_fd, "done", 10, 0, (struct sockaddr *)&client_address, addrlen);
        }
        else if (strcmp(buffer, "asm_short") == 0)
        {
            printf("Got a asm_short request. Proceeding.\n\n");
            for (int i = 0; i < PINGPONG_LEN; i++)
            {
                *((volatile int *)page1) = i;
                *((volatile int *)page2) = i;
            }
            uint64_t x;
            for (int i = 0; i < ASM_REP; i++)
            x = asm_short();
            for (int i = 0; i < PINGPONG_LEN; i++)
            {
                *((volatile int *)page1) = i;
                *((volatile int *)page2) = i;
            }
            printf("%lu\n",x);
            printf("Finished asm_short request. Proceeding.\n\n");
            sendto(listen_fd, "done", 10, 0, (struct sockaddr *)&client_address, addrlen);
        }
        else if (strcmp(buffer, "asm_lsl") == 0)
        {
            printf("Got a asm_lsl request. Proceeding.\n\n");
            for (int i = 0; i < PINGPONG_LEN; i++)
            {
                *((volatile int *)page1) = i;
                *((volatile int *)page2) = i;
            }
            for (int i = 0; i < ASM_REP; i++)
                asm_lsl();
            for (int i = 0; i < PINGPONG_LEN; i++)
            {
                *((volatile int *)page1) = i;
                *((volatile int *)page2) = i;
            }
            printf("Finished asm_short request. Proceeding.\n\n");
            sendto(listen_fd, "done", 10, 0, (struct sockaddr *)&client_address, addrlen);
        }
        else if (strcmp(buffer, "asm_rdrand") == 0)
        {
            printf("Got a asm_rdrand request. Proceeding.\n\n");
            for (int i = 0; i < PINGPONG_LEN; i++)
            {
                *((volatile int *)page1) = i;
                *((volatile int *)page2) = i;
            }
            for (int i = 0; i < ASM_REP; i++)
                asm_rdrand();
            for (int i = 0; i < PINGPONG_LEN; i++)
            {
                *((volatile int *)page1) = i;
                *((volatile int *)page2) = i;
            }
            printf("Finished asm_rdrand request. Proceeding.\n\n");
            sendto(listen_fd, "done", 10, 0, (struct sockaddr *)&client_address, addrlen);
        }
        else if (strcmp(buffer, "asm_nop") == 0)
        {
            printf("Got a asm_nop request. Proceeding.\n\n");
            for (int i = 0; i < PINGPONG_LEN; i++)
            {
                *((volatile int *)page1) = i;
                *((volatile int *)page2) = i;
            }
            for (int i = 0; i < ASM_REP; i++)
                asm_nop();
            for (int i = 0; i < PINGPONG_LEN; i++)
            {
                *((volatile int *)page1) = i;
                *((volatile int *)page2) = i;
            }
            printf("Finished asm_nop request. Proceeding.\n\n");
            sendto(listen_fd, "done", 10, 0, (struct sockaddr *)&client_address, addrlen);
        }
        else if (strcmp(buffer, "get_pages_gpa") == 0)
        {
            printf("Got a gpa request. Sending back gpa1:0x%lx, gpa2:0x%lx\n\n", gpa1, gpa2);
            sprintf(buffer, "gpa1:0x%lx, gpa2:0x%lx\n", gpa1, gpa2);
            sendto(listen_fd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_address, addrlen);
        }
        else if (strcmp(buffer, "stop") == 0 || strcmp(buffer, "exit") == 0)
        {
            break;
        }
        else
        {
            sendto(listen_fd, "NOFUNC", BUFFER_SIZE, 0, (struct sockaddr *)&client_address, addrlen);
        }
    }

    return 0;
}