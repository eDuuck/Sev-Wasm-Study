#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include <time.h>
#include <pthread.h>
#include <unistd.h>

#include "SEVStudy_thread.h"
#include "errorCodes.h"
#include "common_structs.h"

static char UDP_buffer[BUFFER_SIZE];
static int socket_fd;

void rec_mes(){
    if (recvfrom(socket_fd, UDP_buffer, MAXLINE, 0, (struct sockaddr *)NULL, NULL) < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            printf("Timeout reached, no response received.\n");
        } else {
            perror("recvfrom failed");
        }
    }
}

int main(int argc, char **argv)
{
    if(argc < 2){
        perror("Not enough input parameters.");
    }
    
    char *text = argv[2];
    
    
    char *format_prefix = "   ";
    bool debug_enabled = false;
    bool print_meas = true;
    int apic_timer = atoi(argv[1]);
    single_step_measure_args args = {
        .apic_timer = apic_timer,
        .format_prefix = format_prefix,
        .debug_enabled = debug_enabled,
        .print_meas = print_meas};
    
   

    struct sockaddr_in server_address;
    socklen_t addrlen = sizeof(server_address);

    // clear servaddr
    memset(&server_address, 0, addrlen);
    server_address.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_address.sin_port = htons(PORT);
    server_address.sin_family = AF_INET;

    // create datagram socket
    socket_fd = socket(AF_INET, SOCK_DGRAM, 0);

    struct timeval timeout;
    timeout.tv_sec = 0;  // Timeout in seconds
    timeout.tv_usec = 1000*MAX_RUN_TIME; // Timeout in microseconds
    if(setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0)
        perror("setsockopt failed");
    
    // connect to server
    if (connect(socket_fd, (struct sockaddr *)&server_address, addrlen) < 0)
    {
        printf("\n Error : Connect Failed \n");
        exit(0);
    }

    printf("%sChecking if server is active.\n",args.format_prefix);

    sendto(socket_fd, "ping", 10, 0, (struct sockaddr *)NULL, addrlen);
    rec_mes();
    if(strcmp(UDP_buffer,"pong")!=0){
        perror("Didn't get correct respond back from server. Aborting.");
    }

    printf("%sGot response from server!\n",args.format_prefix);

    if(!init_CTX(&args))perror("Init of SEVstep API failed.");
    if(!en_single_step(&args))perror("Init of single-step failed.");


    pthread_t measure_thread;
    pthread_create(&measure_thread,NULL,meas_thread,NULL);
    sendto(socket_fd, text, 10, 0, (struct sockaddr *)NULL, addrlen);
    
    start_meas();

    //usleep(1000000);

    rec_mes();
    for(int i = 0; i < 100; i++){
        stop_meas();
        if(!is_measure_active())
            break;
        usleep(1);
    }
    if (!(errno == EAGAIN || errno == EWOULDBLOCK)) {
        printf("Got answer from server: %s \n",UDP_buffer);
    }
    

    printf("Waiting for thread to finish.");

    
    pthread_join(measure_thread,NULL);
    close_CTX();
    printf("Thread finished. Attempting to contact server.\n");
    sendto(socket_fd, "ping", 10, 0, (struct sockaddr *)NULL, addrlen);
    rec_mes();
    if(strcmp(UDP_buffer,"pong")!=0){
        printf("Didn't get correct response back from server. Oh no.");
    }
    for(int i = 0; i < 100; i++){
        if(close_CTX())
            break;
        usleep(1);
    }
    //pthread_exit(NULL);
    return 0;
}


