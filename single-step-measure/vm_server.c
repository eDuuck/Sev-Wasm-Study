
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

#include "common_structs.h"

#define PORT 8008
#define BUFFER_SIZE 100
#define MAXLINE 1000


#define PAGE_SIZE 4096





// Allocate memory pages
void *allocate_pages(size_t size, int offset) {
    void *mem = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, offset);
    memset(mem, 0, PAGE_SIZE);
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

    void *page1 = allocate_pages(PAGE_SIZE,0);
    void *page2 = allocate_pages(PAGE_SIZE,0);
    uint64_t gpa1 = get_phys_addr(page1);
    uint64_t gpa2 = get_phys_addr(page2);

    printf("Version 5, Pingpong %d now.\n",PINGPONG_LEN);
    
    printf("Worker: Page1 GPA = 0x%lx, Page2 GPA = 0x%lx\n", gpa1, gpa2);
    
    printf("Listening to port %d...\n\n", PORT);
    while (1)
    {
        mess_len = recvfrom(listen_fd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_address, &addrlen);
        buffer[mess_len] = '\0'; // Mark end of string.
        if(strcmp(buffer,"mul")==0){
            a = 5;
            for(volatile int i = 0; i < 100; i++){
                a *= 5;
            }
            snprintf(buffer, BUFFER_SIZE, "%d", a);
            sendto(listen_fd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_address, addrlen);
        }else if(strcmp(buffer,"add")==0){
            a = 5;
            for(volatile int i = 0; i < 100; i++){
                a += 5;
            }
            snprintf(buffer, BUFFER_SIZE, "%d", a);
            sendto(listen_fd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_address, addrlen);
        }else if(strcmp(buffer,"ping")==0){
            printf("Got a ping request.\n\n");
            sendto(listen_fd, "pong", 10, 0, (struct sockaddr *)&client_address, addrlen);
        }else if(strcmp(buffer, "pingpong")==0){
            printf("Got a pingpong request. Proceeding...\n\n");
            for(int i = 0; i < PINGPONG_LEN; i++){
                *((volatile int *)page1) = 42;
                *((volatile int *)page2) = 42;
            }
            printf("Done! Informing client.\n");
            sendto(listen_fd, "done", 10, 0, (struct sockaddr *)&client_address, addrlen);
            
        }
        if(strcmp(buffer,"pingpong add")==0){
            printf("Got a ping_add request. Proceeding.\n\n");
            for(int i = 0; i < PINGPONG_LEN;i++){
                *((volatile int *)page1) = i;
                *((volatile int *)page2) = i;
            }
            for(int i = 0; i < 100; i++){
                a += 5;
            }
            for(int i = 0; i < PINGPONG_LEN;i++){
                *((volatile int *)page1) = i;
                *((volatile int *)page2) = i;
            }
            printf("Finished ping_add request. Proceeding.\n\n");
            sendto(listen_fd, "done", 10, 0, (struct sockaddr *)&client_address, addrlen);
        }else if(strcmp(buffer,"pingpong mul")==0){
            printf("Got a ping_mul request. Proceeding.\n\n");
            for(int i = 0; i < PINGPONG_LEN;i++){
                *((volatile int *)page1) = i;
                *((volatile int *)page2) = i;
            }
            for(int i = 0; i < 100; i++){
                a *= 5;
            }
            for(int i = 0; i < PINGPONG_LEN;i++){
                *((volatile int *)page1) = i;
                *((volatile int *)page2) = i;
            }
            printf("Finished ping_mul request. Proceeding.\n\n");
        }else if(strcmp(buffer,"get_pages_gpa")==0){
            printf("Got a gpa request. Sending back gpa1:0x%lx, gpa2:0x%lx\n\n", gpa1, gpa2);
            sprintf(buffer, "gpa1:0x%lx, gpa2:0x%lx\n", gpa1, gpa2);
            sendto(listen_fd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_address, addrlen);
        }else if(strcmp(buffer,"stop")==0 || strcmp(buffer,"exit")==0) break;
        /*
        sendto(listen_fd, message, MAXLINE, 0, (struct sockaddr *)&client_address, sizeof(client_address));
        char *clientAddr = inet_ntoa(client_address.sin_addr);
        printf("Hello message sent to: %s\n", clientAddr);*/
    }

    return 0;
}