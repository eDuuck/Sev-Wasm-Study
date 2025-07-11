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
    bool print_help = false, TRACK_MEMORY = false;
    int MAX_MEAS_POINTS = 1e7;
    int MAX_MEAS_TIME = 60;
    int APIC_VAL = 0;
    uint64_t ATTACK_PAGE_HPA[2] = {0};

    char note[100] = {'\0'};
    FILE *fptr = NULL;

    usp_event_type_t event_type;
    void *event_buffer = NULL;
    usp_page_fault_event_t *pf_event;
    sev_step_event_t *step_event;

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
    if (ATTACK_PAGE_HPA[0] == 0 || ATTACK_PAGE_HPA[1] == 0 || APIC_VAL == 0)
    {
        print_help = true;
    }
    if (print_help)
    {
        printf("Usage: %s [-t timer_value] [-r {ATTACK_PAGE_HPA}]\n"
               "Other arguments:\n"
               "    -d  :   VM runs in debug mode. (Doesn't seem to work)\n"
               "    -p  :   Print out measurements (enabled by default with -d)\n"
               "    -i  :   Interrupt timer for max measurement time (10s default)\n"
               "    -m  :   Maximum measurement points, 1e7 default.\n",
               argv[0]);
        exit(EXIT_FAILURE);
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
    printf("Proceeding to measurement.\n");

    // Now we start the measurement loop.
    uint64_t curr_ex_gpa = 0, last_ex_gpa = 0, last_mem_gpa = 0; // Need last GPA to retrack old pages.
    bool mem_step = false;

    int steps = 0;
    int last_pf_mode = KVM_PAGE_TRACK_WRITE; // KVM_PAGE_TRACK_WRITE = 0, KVM_PAGE_TRACK_ACCESS = 1, KVM_PAGE_TRACK_EXEC = 3
    char pf_mode_to_char[] = "wa e";         // Whitespace for 2.

    time_t t;
    char fileName[100];
    struct tm *tmp;
    time(&t);
    tmp = localtime(&t);
    strftime(fileName, sizeof(fileName), "output/measurement-%M%S.csv", tmp); // ex: page-track-2837.csv

    fptr = fopen(fileName, "w");
    if (note[0] != '\0')
    {
        fprintf(fptr, "#Note: %s\n", note);
    }
    fprintf(fptr, "Step Event,Latency,Execution Page,Last R/W Pagefault,R/W/E,Counted Instructions,Timestamp[us]\n");
    if (TRACK_MEMORY)
        track_all_pages(&ctx, KVM_PAGE_TRACK_ACCESS); // Track all pages in the VM, so we can measure them.
    else
        TRACK_PAGE(ATTACK_PAGE_HPA[0], KVM_PAGE_TRACK_ACCESS);
    enable_single_stepping(&ctx, APIC_VAL, NULL, 0); // Enable single stepping again to start tracking pages.

    // pthread_t t_timer;
    // pthread_create(&t_timer, NULL, t_timer_interrupt, &MAX_MEAS_TIME);
    // printf("Timer created and entering tracking loop.\n\n");
    time_t start_time = clock();
    // goto cleanup;
    FREE_AND_ACK_EV();

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

            // First we'll write our measurements to file/console.
            if (step_event->counted_instructions > 0)
                fprintf(fptr, "%d,%ld,%lx,%lx,%c,%d,%ld\n",
                        steps,
                        step_event->tsc_latency,
                        curr_ex_gpa,
                        last_mem_gpa,
                        pf_mode_to_char[last_pf_mode],
                        step_event->counted_instructions,
                        clock() - start_time);

            // Then retrack page if write/read. Track exec page if new.

            if (last_pf_mode == KVM_PAGE_TRACK_EXEC && TRACK_MEMORY)
            {
                if (last_ex_gpa != curr_ex_gpa)
                {
                    if (PRINT_MEAS)
                    {
                        printf("Entered new execution page: 0x%lx\n", curr_ex_gpa);
                    }
                    if (last_ex_gpa != 0)
                    {
                        // track_page(&ctx, last_ex_gpa, KVM_PAGE_TRACK_ACCESS); // Reset the execution tracking for the last executed page.
                    }
                    last_ex_gpa = curr_ex_gpa; // Update the last executed GPA.
                }
            }
            else if (step_event->counted_instructions > 0)
            {
                mem_step = true;
                // track_page(&ctx, last_mem_gpa, KVM_PAGE_TRACK_ACCESS); // Reset the write tracking for the last memory page.
            }
            if (PRINT_MEAS)
            {
                printf("Step Event: {GPA:0x%lx}, Latency: %ld, Counted instructions: %d Mode: %c\n",
                       curr_ex_gpa,
                       step_event->tsc_latency,
                       step_event->counted_instructions,
                       pf_mode_to_char[last_pf_mode]);
            }
            last_pf_mode = KVM_PAGE_TRACK_EXEC;        // Reset the last page fault mode to execution.
            steps += step_event->counted_instructions; // Increment the step count by the counted instructions.
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
            if (TRACK_MEMORY)
            {
                if (mem_step)
                {
                    mem_step = false;
                    if (PRINT_MEAS)
                    {
                        printf("Memory step detected, retracking last memory page: 0x%lx\n", last_mem_gpa);
                    }
                    track_all_pages(&ctx, KVM_PAGE_TRACK_ACCESS); // Retrack the last memory page.
                }
                if (pf_event->pf_exec)
                {
                    curr_ex_gpa = pf_event->faulted_gpa; // Save the current execution GPA.
                    if (last_ex_gpa != curr_ex_gpa)
                    {
                        if (PRINT_MEAS)
                        {
                            printf("Entered new execution page: 0x%lx\n", curr_ex_gpa);
                        }
                        if (last_ex_gpa != 0)
                        {
                            track_page(&ctx, last_ex_gpa, KVM_PAGE_TRACK_ACCESS); // Reset the execution tracking for the last executed page.
                        }
                        last_ex_gpa = curr_ex_gpa; // Update the last executed GPA.
                    }
                    last_pf_mode = KVM_PAGE_TRACK_EXEC; // Set the last page fault mode to execution.
                }
                else // Access or Write, could be access to exec page. Then we give access first, then next fault will be execution rights granted.
                {
                    last_mem_gpa = pf_event->faulted_gpa; // Save the last memory GPA.
                    if (pf_event->pf_write)
                    {
                        last_pf_mode = KVM_PAGE_TRACK_WRITE; // Set the last page fault mode to write.
                    }
                    else
                    {
                        last_pf_mode = KVM_PAGE_TRACK_ACCESS; // Set the last page fault mode to access.
                    }
                }
                if (PRINT_MEAS)
                {
                    printf("Pagefault Event: {GPA:0x%lx}, Mode: %c\n",
                           pf_event->faulted_gpa,
                           pf_mode_to_char[last_pf_mode]);
                }
            }
        }
        FREE_AND_ACK_EV();
    }

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