#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

#include <getopt.h>

#include <stdatomic.h>
#include <time.h>
#include <pthread.h>

#include "../sev-step-lib/ansi-color-codes.h"
#include "../sev-step-lib/sev_step_api.h"

#include "target-measurement.h"
#include "server/vm-constants.h"

#define LOG_FILE_PATH "output/measurement_logs.log"

#define FREE_AND_ACK_EV()                     \
    usp_ack_event(&ctx);                      \
    free_usp_event(event_type, event_buffer); \
    event_buffer = NULL

#define TRACK_PAGE(gpa, mode)                                                          \
    if (SEV_STEP_OK != track_page(&ctx, gpa, mode))                                    \
    {                                                                                  \
        printf(BRED "Failed to track page 0x%lx with mode %d." reset "\n", gpa, mode); \
        goto cleanup;                                                                  \
    }

static atomic_bool should_run = true;
static bool kill_timer = false; // Used to signal the timer thread to exit.

struct measurement_data
{
    int step_event;
    long latency;
    uint64_t execution_page;
    uint64_t last_rw_pagefault;
    char rw_mode; // 'w' for write, 'a' for access, 'e' for execute
    int counted_instructions;
    long timestamp; // Timestamp in microseconds
};

void *t_timer_interrupt(void *arg)
{
    int time = *(int *)arg;
    double elapsed_time, lastPrint = 0.0;
    time_t start_time = clock();
    // struct timespec ts = {0, 10 * 1000000}; // 10 ms
    do
    {
        elapsed_time = (double)(clock() - start_time) / CLOCKS_PER_SEC;
        if ((remainder(elapsed_time, 5)) < 1 &&
            (remainder(elapsed_time, 5)) > 0 &&
            (elapsed_time - lastPrint) > 1)
        {
            printf("Elapsed time: %d / %d.\n", (int)elapsed_time, time);
            lastPrint = elapsed_time;
        }
        if (kill_timer)
        {
            return NULL; // Exit if the main thread has set should_run to false.
        }
    } while (elapsed_time < time);
    printf("Elapsed time: %d / %d.\n", time, time);
    atomic_exchange(&should_run, false);
    return NULL;
}

bool should_abort()
{
    return !atomic_load(&should_run);
}

int main(int argc, char **argv)
{
    bool DEBUG_EN = false, PRINT_MEAS = false;
    bool print_help = false, TRACK_MEMORY = true;
    uint64_t MAX_MEAS_POINTS = 5e9;// 1e9 ~50GB of data
    int MAX_MEAS_TIME = 60;
    int APIC_VAL = 0;
    int flush_counter = 0;
    uint64_t ATTACK_PAGE_HPA[2] = {0};

    char note[100] = {'\0'};
    FILE *fptr = NULL;

    usp_event_type_t event_type;
    void *event_buffer = NULL;
    usp_page_fault_event_t *pf_event;
    sev_step_event_t *step_event;
    int event_steps = 0;

    int opt;
    while ((opt = getopt(argc, argv, "m:t:i:r:pdhn:")) != -1)
    {
        switch (opt)
        {
        case 'm':
            MAX_MEAS_POINTS = atoi(optarg);
            break;
        case 't':
            APIC_VAL = atoi(optarg);
            break;
        case 'i':
            MAX_MEAS_TIME = atoi(optarg);
            break;
        case 'd':
            DEBUG_EN = true;
            PRINT_MEAS = true;
            break;
        case 'p':
            PRINT_MEAS = true;
            break;
        case 'r':
        {
            // Support two addresses separated by '-'
            char *dash = strchr(optarg, '-');
            if (dash)
            {
                *dash = '\0';
                ATTACK_PAGE_HPA[0] = strtoull(optarg, NULL, 0) * 0x1000;
                ATTACK_PAGE_HPA[1] = strtoull(dash + 1, NULL, 0) * 0x1000;
                break;
            }
        }
        case 'n':
            if (strlen(optarg) < 100)
            {
                strncpy(note, optarg, 100);
            }
            else
            {
                printf("Note is too long, max 100 characters.\n");
                exit(EXIT_FAILURE);
            }
            break;
        default:
            print_help = true;
            break;
        }
    }
    if (ATTACK_PAGE_HPA[0] == 0 || ATTACK_PAGE_HPA[1] == 0 || APIC_VAL == 0 || note[0] == '\0')
    {
        print_help = true;
    }
    if (print_help)
    {
        printf("Usage: %s [-t timer_value] [-n {RESULT_FILE_NAME}] [-r {ATTACK_PAGE_HPA}]\n"
               "Other arguments:\n"
               "    -d  :   VM runs in debug mode. (Doesn't seem to work)\n"
               "    -p  :   Print out measurements (enabled by default with -d)\n"
               "    -i  :   Interrupt timer for max measurement time (10s default)\n"
               "    -m  :   Maximum measurement points, 1e7 default.\n",
               argv[0]);
        exit(EXIT_FAILURE);
    }
    

    char fileName[FILENAME_CHAR_LIM] = "output/";
    for(int i = 0; note[i] != '\0'; i++)
    {
        if(i + 7 >= FILENAME_CHAR_LIM){
            printf("Too long filename: exiting.\n");
            exit(EXIT_FAILURE);
        }
        fileName[i + 7] = note[i];
    }

    printf("Attack pages specified at: 0x%lx and 0x%lx\n", ATTACK_PAGE_HPA[0], ATTACK_PAGE_HPA[1]);

    usp_poll_api_ctx_t ctx;
    bool api_open = false;
    if (SEV_STEP_ERR == usp_new_ctx(&ctx, DEBUG_EN))
    {
        printf(BRED "usp_new_ctx failed." reset " Check dmesg for more information\n");
        goto cleanup;
    }
    printf("CTX initiated.\n");
    api_open = true;

    TRACK_PAGE(ATTACK_PAGE_HPA[0], KVM_PAGE_TRACK_WRITE);
    int ret;
    printf("Waiting for pagefault event at GPA: 0x%lx\n", ATTACK_PAGE_HPA[0]);
    ret = usp_block_until_event(&ctx, &event_type, &event_buffer);
    if (ret == SEV_STEP_ERR_ABORT || ret == SEV_STEP_ERR || event_type != PAGE_FAULT_EVENT)
    {
        printf("Exiting program!\n");
        event_buffer = NULL;
        goto cleanup;
    }
    pf_event = (usp_page_fault_event_t *)event_buffer;
    printf("Detected write at GPA: 0x%lx\n", pf_event->faulted_gpa);

    TRACK_PAGE(ATTACK_PAGE_HPA[1], KVM_PAGE_TRACK_WRITE);
    FREE_AND_ACK_EV();
    ret = usp_block_until_event(&ctx, &event_type, &event_buffer);
    if (ret == SEV_STEP_ERR_ABORT || ret == SEV_STEP_ERR || event_type != PAGE_FAULT_EVENT)
    {
        printf("Exiting program!\n");
        event_buffer = NULL;
        goto cleanup;
    }
    pf_event = (usp_page_fault_event_t *)event_buffer;
    printf("Detected write at GPA: 0x%lx\n", pf_event->faulted_gpa);
    printf("Tracking attack pages done.\n");

    // Used for page fault tracking.
    uint64_t curr_ex_gpa = 0;
    uint64_t pf_arr[PF_LIMIT] = {0};
    char pf_mode_arr[PF_LIMIT];
    int32_t pf_count = 0;

    uint64_t steps = 0;
    char pf_mode_to_char[] = "wa e"; // Whitespace for 2.   KVM_PAGE_TRACK_WRITE = 0, KVM_PAGE_TRACK_ACCESS = 1, KVM_PAGE_TRACK_EXEC = 3

    fptr = fopen(fileName, "w");
    fprintf(fptr, "Steps,Latency,Execution Page,Last PF Mode,PF num,Last PF,Counted Instructions,Timestamp[us]");
    if (TRACK_MEMORY)
        track_all_pages(&ctx, KVM_PAGE_TRACK_ACCESS);
    else
        TRACK_PAGE(ATTACK_PAGE_HPA[0], KVM_PAGE_TRACK_ACCESS);
    enable_single_stepping(&ctx, APIC_VAL, NULL, 0);


    time_t meas_start = time(NULL);
    // pthread_t t_timer;
    // pthread_create(&t_timer, NULL, t_timer_interrupt, &MAX_MEAS_TIME);
    // printf("Timer created and entering tracking loop.\n\n");
    time_t start_time = clock();
    FREE_AND_ACK_EV();
    printf("Proceeding to measurement.\n");
    while (steps < MAX_MEAS_POINTS) // && atomic_load(&should_run))
    {
        // ret = usp_block_until_event_or_cb(&ctx, &event_type, &event_buffer, should_abort, NULL);
        ret = usp_block_until_event(&ctx, &event_type, &event_buffer);
        if (ret == SEV_STEP_ERR || ret == SEV_STEP_ERR_ABORT)
        {
            printf("Exiting program!\n");
            goto cleanup;
        }
        if (event_type == SEV_STEP_EVENT) // We have performed a step.
        {
            step_event = (sev_step_event_t *)event_buffer;
            event_steps = (int)step_event->counted_instructions;
            if (event_steps == -1)
            {
                printf("Weird step event, step %lu.\n", steps);
                // FREE_AND_ACK_EV();
                // goto cleanup;
            }
            else if (event_steps > 0 && event_steps < MULTISTEP_LIMIT)
            {
                steps += event_steps;
                if (TRACK_MEMORY)
                {
                    fprintf(fptr, "\n%lu,%ld,0x%lx,%c,%d,",
                            steps,
                            step_event->tsc_latency,
                            curr_ex_gpa,
                            pf_mode_arr[pf_count - 1],
                            pf_count);
                    /*for (int i = pf_count - 1; i > 0; i--)
                    {
                        fprintf(fptr, "0x%lx-%c;", pf_arr[i], pf_mode_arr[i]);
                    }
                    fprintf(fptr, "0x%lx-%c,", pf_arr[0], pf_mode_arr[0]);*/
                    fprintf(fptr, "0x%lx-%c,", pf_arr[pf_count - 1], pf_mode_arr[pf_count - 1]);
                    fprintf(fptr, "%d,%ld",
                            event_steps,
                            clock() - start_time);
                    flush_counter++;
                    if(flush_counter > FLUSH_LIM){
                        fflush(fptr);
                        flush_counter = 0;
                    }
                    for (int i = pf_count - 1; i >= 0; i--)
                    {
                        TRACK_PAGE(pf_arr[i], KVM_PAGE_TRACK_ACCESS)
                    }
                    pf_count = 0;
                }
                else
                {
                    fprintf(fptr, "\n%lu,%ld,NA,NA,NA,NA,%d,%ld",
                            steps,
                            step_event->tsc_latency,
                            event_steps,
                            clock() - start_time);
                }
                if (PRINT_MEAS)
                {
                    printf("Step Event: {GPA:0x%lx}, Latency: %ld, Counted instructions: %d\n",
                           curr_ex_gpa,
                           step_event->tsc_latency,
                           event_steps);
                }
            }else if(event_steps >= MULTISTEP_LIMIT){
                printf("Overstepped step event, step %lu with %d instructions.\n", steps, event_steps);
                FREE_AND_ACK_EV();
                break;
            }
        }
        else if (event_type == PAGE_FAULT_EVENT)
        {
            pf_event = (usp_page_fault_event_t *)event_buffer;
            if (pf_event->faulted_gpa == ATTACK_PAGE_HPA[0])
            {
                printf("Attack page accessed at: 0x%lx\n", pf_event->faulted_gpa);
                FREE_AND_ACK_EV(); // Acknowledge the page fault event.
                break;             // Exit the loop if the attack page is accessed.
            }

            pf_arr[pf_count] = pf_event->faulted_gpa;
            if (pf_event->pf_exec)
            {
                pf_mode_arr[pf_count] = 'E';
                curr_ex_gpa = pf_event->faulted_gpa;
            }
            else if (pf_event->pf_write)
            {
                pf_mode_arr[pf_count] = 'W';
            }
            else
            {
                pf_mode_arr[pf_count] = 'A';
            }
            pf_count++;

            if (PRINT_MEAS)
            {
                printf("Pagefault Event: {GPA:0x%lx}, Mode: %c\n",
                       pf_event->faulted_gpa,
                       pf_mode_to_char[pf_event->pf_mode]);
            }
        }
        FREE_AND_ACK_EV();
    }

    FILE *log_file = fopen(LOG_FILE_PATH, "a");
    if (!log_file) {
        perror("Failed to open log file");
        goto cleanup;
    }

    time_t meas_end = time(NULL);
    
    struct tm s_t = *localtime(&meas_start);
    struct tm e_t = *localtime(&meas_end);
    fprintf(log_file, "[%02d-%02d %02d:%02d:%02d] - [%02d-%02d %02d:%02d:%02d]: %s finished with %lu recorded steps.\n",
            s_t.tm_mon + 1, s_t.tm_mday,
            s_t.tm_hour, s_t.tm_min, s_t.tm_sec,
            e_t.tm_mon + 1, e_t.tm_mday,
            e_t.tm_hour, e_t.tm_min, e_t.tm_sec,
            note, steps);
    fclose(log_file);

cleanup:
    if (api_open)
    {
        if (SEV_STEP_ERR == usp_close_ctx(&ctx)) // Close CTX untracks all pages and disables single stepping.
        {
            printf(BRED "usp_close_ctx failed." reset " Check dmesg for more information\n");
            return 0;
        }
        printf("Closed CTX.\n");
    }
    if (event_buffer != NULL)
    {
        free_usp_event(event_type, event_buffer);
    }

    if (fptr != NULL)
    {
        fclose(fptr);
        printf("Measurements written to %s\n", fileName);
    }

    kill_timer = true; // Signal the timer thread to exit.
    atomic_exchange(&should_run, false);
    // pthread_join(t_timer, NULL);
    printf("Quitting program.\n");
    return 1;
}
