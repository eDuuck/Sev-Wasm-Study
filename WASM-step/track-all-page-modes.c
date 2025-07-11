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

#include <unistd.h>

#include "../sev-step-lib/ansi-color-codes.h"
#include "../sev-step-lib/sev_step_api.h"

static atomic_bool should_run = false;

struct fault_data
{
    uint64_t gpa;
    int mode;
    double time;
    int repetition;
};

void *interrupt_thread(void *arg)
{
    int time = *(int *)arg;
    double elapsed_time, lastPrint = 0.0;
    time_t start_time = clock();
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
        if (atomic_load(&should_run) == false)
        {
            return NULL;
        }
        usleep(1000); // Sleep for 1ms to prevent busy waiting
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
    bool debug_enabled = false;
    bool print_meas = false;
    int measurement_points = 1e4;
    int max_measure_time = 30;
    int opt;
    int repetitions = 5;

    while ((opt = getopt(argc, argv, "m:t:r:dp")) != -1)
    {
        switch (opt)
        {
        case 'm':
            measurement_points = atoi(optarg);
            break;
        case 't':
            max_measure_time = atoi(optarg);
            break;
        case 'r':
            repetitions = atoi(optarg);
            break;
        case 'd':
            debug_enabled = true;
            print_meas = true;
            break;
        case 'p':
            print_meas = true;
            break;
        default:
            printf("Usage: Read the code and figure it out.\n");
            exit(EXIT_FAILURE);
        }
    }
    time_t t;
    char fileName[100];
    struct tm *tmp;
    time(&t);
    tmp = localtime(&t);
    strftime(fileName, sizeof(fileName), "output/page-track-%d%H%M%S.csv", tmp); // ex: page-track-27152837.csv

    FILE *fptr;
    fptr = fopen(fileName, "w");

    fprintf(fptr, "Page Access,Timestamp,Track Mode,PF_type,Track Repetition\n");

    usp_poll_api_ctx_t ctx;
    if (SEV_STEP_ERR == usp_new_ctx(&ctx, debug_enabled))
    {
        printf(BRED "usp_new_ctx failed." reset " Check dmesg for more information\n");
        return 0;
    }
    printf("CTX set up, setting interrupt timer to %ds.\n", max_measure_time);

    int collected_measurements = 0;

    pthread_t t_timer;

    usp_event_type_t event_type;
    void *event_buffer;
    time_t start_time;
    usp_page_fault_event_t *pf_event;
    int ret;
    char *pf_type = "wa_e";
    printf("Entering tracking loop.\n");
    uint64_t acc_page = 0;
    for (int j = 0; j < repetitions; j++)
    {
        printf("Entering loop %d/%d.\n", repetitions, repetitions);
        track_all_pages(&ctx, KVM_PAGE_TRACK_ACCESS);
        atomic_exchange(&should_run, true);
        pthread_create(&t_timer, NULL, interrupt_thread, &max_measure_time);
        start_time = clock();
        do
        {
            ret = usp_block_until_event_or_cb(&ctx, &event_type, &event_buffer, should_abort, NULL);
            if (ret == SEV_STEP_ERR_ABORT || ret == SEV_STEP_ERR)
            {
                printf("Abort initiated! Untracking pages and exiting!\n");
                goto close;
            }

        acc_fault:
            pf_event = (usp_page_fault_event_t *)event_buffer;
            acc_page = pf_event->faulted_gpa;
            struct fault_data fault_d = {
                .gpa = acc_page,
                .mode = pf_event->fault_mode,
                .time = ((double)(clock() - start_time) / CLOCKS_PER_SEC),
                .repetition = j + 1,
            };

            if(pf_event->fault_mode == KVM_PAGE_TRACK_WRITE)
                goto w_fault; // If it was a write fault, we will handle it later.
            if(pf_event->fault_mode == KVM_PAGE_TRACK_EXEC)
                goto ex_fault; // If it was an execution fault, we will handle it later.

            //track_page(&ctx, acc_page, KVM_PAGE_TRACK_EXEC);
            track_page(&ctx, acc_page, KVM_PAGE_TRACK_WRITE);
            
            usp_ack_event(&ctx);
            free_usp_event(event_type, event_buffer);

            ret = usp_block_until_event_or_cb(&ctx, &event_type, &event_buffer, should_abort, NULL);
            if (ret == SEV_STEP_ERR_ABORT || ret == SEV_STEP_ERR)
            {
                printf("Abort initiated! Untracking pages and exiting!\n");
                goto close;
            }

            pf_event = (usp_page_fault_event_t *)event_buffer;
            if (pf_event->fault_mode == KVM_PAGE_TRACK_EXEC) // It was a page execution
            {
                ex_fault:
                fault_d.mode = KVM_PAGE_TRACK_EXEC;
                //untrack_page(&ctx, acc_page, KVM_PAGE_TRACK_WRITE);
            }
            else if (pf_event->fault_mode == KVM_PAGE_TRACK_WRITE) // Second priority
            {
                w_fault:
                fault_d.mode = KVM_PAGE_TRACK_WRITE;
                //untrack_page(&ctx, acc_page, KVM_PAGE_TRACK_EXEC);
            }
            fprintf(fptr, "%lx,%lf,%c,%d\n",
                    fault_d.gpa,
                    fault_d.time,
                    pf_type[fault_d.mode],
                    fault_d.repetition);
            collected_measurements++;
            if(print_meas)
            {
                printf("Pagefault Event: {GPA:0x%lx}, Mode: %d, Time: %lf, Repetition: %d\n",
                       fault_d.gpa, fault_d.mode, fault_d.time, fault_d.repetition);
            }
            if (pf_event->fault_mode == KVM_PAGE_TRACK_ACCESS) // It was actually just a page read.
            {
                goto acc_fault;
            }

            usp_ack_event(&ctx);
            free_usp_event(event_type, event_buffer);

        } while (atomic_load(&should_run) && collected_measurements < measurement_points);

        untrack_all_pages(&ctx, KVM_PAGE_TRACK_ACCESS);
        untrack_all_pages(&ctx, KVM_PAGE_TRACK_WRITE);
        untrack_all_pages(&ctx, KVM_PAGE_TRACK_EXEC);
        pthread_join(t_timer, NULL);
        if (collected_measurements >= measurement_points)
            break;
    }

    printf("Exited measurement loop, finishing up.\n");

close:
    if (SEV_STEP_ERR == usp_close_ctx(&ctx))
    {
        printf(BRED "usp_close_ctx failed." reset " Check dmesg for more information\n");
        return 0;
    }
    fclose(fptr);
    // Signal the timer thread to exit early if it's still running
    atomic_exchange(&should_run, false);
    pthread_join(t_timer, NULL);
    return 1;
}