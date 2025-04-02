
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/socket.h>
#include <sys/mman.h>
#include <sys/types.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#define PORT 8008
#define BUFFER_SIZE 100
#define MAXLINE 1000


#define PAGE_SIZE 4096



// Allocate memory pages
void *allocate_pages(size_t size) {
    void *mem = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) {
        perror("mmap failed");
        exit(EXIT_FAILURE);
    }
    return mem;
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

    void *page1 = allocate_pages(PAGE_SIZE);
    void *page2 = allocate_pages(PAGE_SIZE);

    printf("Listening to port %d...\n", PORT);
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
        }else if(strcmp(buffer,"add")==0){
            a = 5;
            for(volatile int i = 0; i < 100; i++){
                a += 5;
            }
        }else if(strcmp(buffer,"ping")==0){
            sendto(listen_fd, "pong", 10, 0, (struct sockaddr *)&client_address, addrlen);
        }else if(strcmp(buffer, "pingpong")){
            for(int i = 0; i < 100; i++){
                *((volatile int *)page1) = 42;
                *((volatile int *)page2) = 42;
            }
            sendto(listen_fd, "done", 10, 0, (struct sockaddr *)&client_address, addrlen);
            
        }
        if(strcmp(buffer,"pingpong add")==0){
            for(int i = 0; i < 10;i++){
                *((volatile int *)page1) = 42;
                *((volatile int *)page2) = 42;
            }
            for(int i = 0; i < 100; i++){
                a += 5;
            }
            for(int i = 0; i < 10;i++){
                *((volatile int *)page1) = 42;
                *((volatile int *)page2) = 42;
            }
        }else if(strcmp(buffer,"pingpong mul")==0){
            for(int i = 0; i < 10;i++){
                *((volatile int *)page1) = 42;
                *((volatile int *)page2) = 42;
            }
            for(int i = 0; i < 100; i++){
                a *= 5;
            }
            for(int i = 0; i < 10;i++){
                *((volatile int *)page1) = 42;
                *((volatile int *)page2) = 42;
            }
        }else if(strcmp(buffer,"stop")==0 || strcmp(buffer,"exit")==0) break;
        /*
        sendto(listen_fd, message, MAXLINE, 0, (struct sockaddr *)&client_address, sizeof(client_address));
        char *clientAddr = inet_ntoa(client_address.sin_addr);
        printf("Hello message sent to: %s\n", clientAddr);*/
    }

    return 0;
}