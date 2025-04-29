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

#include "../sev-step-lib/sev_step_api.h"

#include "SEVStudy_thread.h"
#include "errorCodes.h"
#include "common_structs.h"

static char UDP_buffer[BUFFER_SIZE];
static int socket_fd;
static char *format_prefix = "   ";

void rec_mes(bool print){
    if (recvfrom(socket_fd, UDP_buffer, MAXLINE, 0, (struct sockaddr *)NULL, NULL) < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            printf("Timeout reached, no response received.\n");
        } else {
            perror("recvfrom failed");
        }
    }else if(print){
        printf("%sGot answer from server: %s \n",format_prefix,UDP_buffer);
    }
}

int main(int argc, char **argv)
{
    if(argc < 3){
        printf("Usage: %s apic_timer_value server_comm\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    //char *text = argv[2];
    
    //char *format_prefix = "   ";
    bool debug_enabled = false;
    bool print_meas = true;
    int apic_timer = atoi(argv[1]);
    single_step_measure_args args = {
        .apic_timer = apic_timer,
        .format_prefix = format_prefix,
        .debug_enabled = debug_enabled,
        .print_meas = print_meas,
        .call_fun = argv[2],
        .call_fun_len = sizeof(argv[2]),
    };
    

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
    timeout.tv_sec = MAX_RUN_TIME;  // Timeout in seconds
    timeout.tv_usec = 0; // Timeout in microseconds
    if(setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0)
        perror("setsockopt failed");
    
    // connect to server
    if (connect(socket_fd, (struct sockaddr *)&server_address, addrlen) < 0)
    {
        printf("\n Error : Connect Failed \n");
        exit(0);
    }

    printf("%sStarting test, max single-steps: %d.\n\n",args.format_prefix,MAX_STEP_AMOUNT);

    printf("%sChecking if server is active.\n",args.format_prefix);

    sendto(socket_fd, "ping", BUFFER_SIZE, 0, (struct sockaddr *)NULL, addrlen);
    rec_mes(false);
    if(strcmp(UDP_buffer,"pong")!=0){
        printf("Didn't get correct respond back from server. Aborting.\n");
        exit(1);
    }

    printf("%sGot response from server!\n",args.format_prefix);

    printf("%sChecking if function available.\n",args.format_prefix);
    
    sendto(socket_fd, args.call_fun, BUFFER_SIZE, 0, (struct sockaddr *)NULL, addrlen);
    rec_mes(true);
    if(strcmp(UDP_buffer,"NOFUNC")==0 || (errno == EAGAIN || errno == EWOULDBLOCK)){
        printf("Server is missing function, or not responding. Aborting.\n");
        exit(1);
    }
    printf("%sServer can perform requested function call.\n",args.format_prefix);

    printf("%sAsking for pingpong GPAs.\n",args.format_prefix);

    sendto(socket_fd, "get_pages_gpa", BUFFER_SIZE, 0, (struct sockaddr *)NULL, addrlen);
    rec_mes(true);

    uint64_t gpa1, gpa2;
    sscanf(UDP_buffer, "gpa1:0x%lx, gpa2:0x%lx", &gpa1, &gpa2);

    if(!init_CTX(&args))perror("Init of SEVstep API failed.");
    printf("%sCTX set up!\n",args.format_prefix);
    set_gpas(gpa1,gpa2);

    pthread_t monitor_thread;
    pthread_create(&monitor_thread,NULL,inside_pingpong_measure,NULL);
    
    printf("%sCalling function from server!\n",args.format_prefix);
    start_meas();
    sendto(socket_fd, args.call_fun, BUFFER_SIZE, 0, (struct sockaddr *)NULL, addrlen);
    
    rec_mes(true);
    for(int i = 0; i < 100; i++){
        stop_meas();
        if(!is_measure_active())
            break;
    }

    printf("%sWaiting for thread to finish.\n",args.format_prefix);

    
    pthread_join(monitor_thread,NULL);
    printf("%sThread finished. Attempting to contact server.\n",args.format_prefix);
    close_CTX();
    sendto(socket_fd, "ping", BUFFER_SIZE, 0, (struct sockaddr *)NULL, addrlen);
    rec_mes(true);
    if(strcmp(UDP_buffer,"pong")!=0){
        printf("Didn't get correct response back from server. Oh no.\n");
        /*for(int i = 0; i < 100; i++){
            if(close_CTX())
                break;
            usleep(1);
        }*/
    }else printf("%sGot response!\n",args.format_prefix);
    
    
    //pthread_exit(NULL);
    return 0;
}


