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

/* --- PRINTF_BYTE_TO_BINARY macro's --- */
#define PRINTF_BINARY_PATTERN_INT8 "%c%c%c%c%c%c%c%c"
#define PRINTF_BYTE_TO_BINARY_INT8(i) \
    (((i) & 0x80ll) ? '1' : '0'),     \
        (((i) & 0x40ll) ? '1' : '0'), \
        (((i) & 0x20ll) ? '1' : '0'), \
        (((i) & 0x10ll) ? '1' : '0'), \
        (((i) & 0x08ll) ? '1' : '0'), \
        (((i) & 0x04ll) ? '1' : '0'), \
        (((i) & 0x02ll) ? '1' : '0'), \
        (((i) & 0x01ll) ? '1' : '0')

#define PRINTF_BINARY_PATTERN_INT16 \
    PRINTF_BINARY_PATTERN_INT8 PRINTF_BINARY_PATTERN_INT8
#define PRINTF_BYTE_TO_BINARY_INT16(i) \
    PRINTF_BYTE_TO_BINARY_INT8((i) >> 8), PRINTF_BYTE_TO_BINARY_INT8(i)
#define PRINTF_BINARY_PATTERN_INT32 \
    PRINTF_BINARY_PATTERN_INT16 PRINTF_BINARY_PATTERN_INT16
#define PRINTF_BYTE_TO_BINARY_INT32(i) \
    PRINTF_BYTE_TO_BINARY_INT16((i) >> 16), PRINTF_BYTE_TO_BINARY_INT16(i)
#define PRINTF_BINARY_PATTERN_INT64 \
    PRINTF_BINARY_PATTERN_INT32 PRINTF_BINARY_PATTERN_INT32
#define PRINTF_BYTE_TO_BINARY_INT64(i) \
    PRINTF_BYTE_TO_BINARY_INT32((i) >> 32), PRINTF_BYTE_TO_BINARY_INT32(i)
/* --- end macros --- */

static atomic_bool should_run = false;

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
        // usleep(1000); // Sleep for 1ms to prevent busy waiting
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

    fprintf(fptr, "Timestamp,PF_type,pf_exec,pf_write,pf_present,Track Repetition,Page Access\n");

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
    int track_mode;
    char track_char;
    char *pf_type = "wa_e";
    printf("Entering tracking loop.\n");
    for (int i = 0; i < 3; i++)
    {
        switch (2-i)
        {
        case 0:
            track_mode = KVM_PAGE_TRACK_ACCESS;
            track_char = 'a';
            break;
        case 1:
            track_mode = KVM_PAGE_TRACK_WRITE;
            track_char = 'w';
            break;
        case 2:
            track_mode = KVM_PAGE_TRACK_EXEC;
            track_char = 'e';
            break;
        default:
            printf("Unexpected program behaviour, exiting.\n");
            goto close;
        }
        for (int j = 0; j < repetitions; j++)
        {
            printf("Entering loop %d/%d, measuring %c.\n", i * repetitions + j + 1, 3 * repetitions, track_char);
            track_all_pages(&ctx, track_mode);
            // track_all_pages(&ctx, KVM_PAGE_TRACK_EXEC);
            // track_all_pages(&ctx, KVM_PAGE_TRACK_WRITE);
            // track_all_pages(&ctx, KVM_PAGE_TRACK_ACCESS);
            atomic_exchange(&should_run, true);
            pthread_create(&t_timer, NULL, interrupt_thread, &max_measure_time);
            start_time = clock();
            do
            {
                int ret = usp_block_until_event_or_cb(&ctx, &event_type, &event_buffer, should_abort, NULL);
                if (ret == SEV_STEP_ERR_ABORT)
                    break;
                if (ret == SEV_STEP_ERR)
                {
                    printf("Abort initiated! Untracking pages and exiting!\n");
                    goto close;
                }
                else if (event_type != PAGE_FAULT_EVENT)
                {
                    printf("Unexpected event type! Untracking pages and exiting!\n");
                    usp_ack_event(&ctx);
                    free_usp_event(event_type, event_buffer);
                    goto close;
                }
                else
                {
                    collected_measurements++;
                    usp_page_fault_event_t *pf_event = (usp_page_fault_event_t *)event_buffer;
                    uint64_t pf_gpa = pf_event->faulted_gpa;
                    if (print_meas)
                        printf("Pagefault Event: {GPA:0x%lx}, Mode: %d\n", pf_gpa, pf_event->pf_mode);
                    fprintf(fptr, "%f,%c,%d,%d,%d,%d,%ld\n",
                            ((double)(clock() - start_time) / CLOCKS_PER_SEC),
                            pf_type[pf_event->pf_mode],
                            pf_event->pf_exec,
                            pf_event->pf_write,
                            pf_event->pf_present,
                            i * repetitions + j + 1,
                            (long)pf_gpa);
                }
                free_usp_event(event_type, event_buffer);
                usp_ack_event(&ctx);
            } while (atomic_load(&should_run) && collected_measurements < measurement_points);
            atomic_exchange(&should_run, false);
            untrack_all_pages(&ctx, track_mode);
            // untrack_all_pages(&ctx, KVM_PAGE_TRACK_ACCESS);
            // untrack_all_pages(&ctx, KVM_PAGE_TRACK_WRITE);
            // untrack_all_pages(&ctx, KVM_PAGE_TRACK_EXEC);
            pthread_join(t_timer, NULL);
            if (collected_measurements >= measurement_points)
                break;
        }
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
    pthread_join(t_timer, NULL);
    return 1;
}