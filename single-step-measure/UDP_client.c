// udp client driver program
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#define PORT 8008
#define BUFFER_SIZE 100
#define MAXLINE 1000

// Driver code
int main()
{
    char buffer[BUFFER_SIZE];
    char message[MAXLINE];
    int socket_fd;
    struct sockaddr_in server_address;
    socklen_t addrlen = sizeof(server_address);

    // clear servaddr
    memset(&server_address, 0, addrlen);
    server_address.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_address.sin_port = htons(PORT);
    server_address.sin_family = AF_INET;

    // create datagram socket
    socket_fd = socket(AF_INET, SOCK_DGRAM, 0);

    // connect to server
    if (connect(socket_fd, (struct sockaddr *)&server_address, addrlen) < 0)
    {
        printf("\n Error : Connect Failed \n");
        exit(0);
    }
    while (1)
    {
        printf("Write a message to send to the server. Enter 'stop' to exit.\n");
        scanf(" %[^\n]s", message);
        // request to send datagram
        // no need to specify server address in sendto
        // connect stores the peers IP and port
        sendto(socket_fd, message, MAXLINE, 0, (struct sockaddr *)NULL, addrlen);

        // We exit after informing server to stop.
        if (strcmp(message, "stop") == 0 || strcmp(message, "exit") == 0)
            break;

        printf("\n");
        // waiting for response
        //recvfrom(socket_fd, buffer, sizeof(buffer), 0, (struct sockaddr *)NULL, NULL);
        //printf("%s \n\n",buffer);
    }
    // close the descriptor
    close(socket_fd);


    return 0;
}
