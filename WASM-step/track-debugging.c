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

int main(int argc, char **argv)
{
    bool DEBUG_EN = false;
    bool print_help = true;
    int APIC_VAL = 0;

    usp_event_type_t event_type;
    void *event_buffer = NULL;
    sev_step_event_t *step_event;

    int opt;
    while ((opt = getopt(argc, argv, "t:")) != -1)
    {
        switch (opt)
        {
        case 't':
            APIC_VAL = atoi(optarg);
            print_help = false;
            break;
        default:
            break;
        }
    }
    if (print_help)
    {
        printf("Usage: %s [-t timer_value]\n"
               "Other arguments:\n"
               "    -p  :   Print out measurements.\n",
               argv[0]);
        exit(EXIT_FAILURE);
    }

    usp_poll_api_ctx_t ctx;
    bool api_open = false;
    if (SEV_STEP_ERR == usp_new_ctx(&ctx, DEBUG_EN))
    {
        printf(BRED "usp_new_ctx failed." reset " Check dmesg for more information\n");
        goto cleanup;
    }
    printf("CTX initiated.\n");
    api_open = true;

    time_t start_time = clock();
    printf("%11.8f:Entering loop of track/untrack all pages.\n", (double)(clock() - start_time) / CLOCKS_PER_SEC);
    for (int i = 0; i < 3; i++)
    {
        if (SEV_STEP_OK != track_all_pages(&ctx, KVM_PAGE_TRACK_ACCESS))
        {
            printf("%11.8f:Failed to track all pages.\n", (double)(clock() - start_time) / CLOCKS_PER_SEC);
            goto cleanup;
        }
        printf("%11.8f:Tracked_all_pages().\n", (double)(clock() - start_time) / CLOCKS_PER_SEC);
        if (SEV_STEP_OK != untrack_all_pages(&ctx, KVM_PAGE_TRACK_ACCESS))
        {
            printf("%11.8f:Failed to untrack all pages.\n", (double)(clock() - start_time) / CLOCKS_PER_SEC);
            goto cleanup;
        }
        printf("%11.8f:Untracked_all_pages().\n", (double)(clock() - start_time) / CLOCKS_PER_SEC);
    }

    printf("%11.8f:Finished track/untrack loop, proceeding tracking single-stepping loop.\n", (double)(clock() - start_time) / CLOCKS_PER_SEC);

    enable_single_stepping(&ctx, APIC_VAL, NULL, 0); // Enable single stepping again to start tracking pages.

    int steps = 0;
    int ret;
    printf("%11.8f:Proceeding to measurement.\n", (double)(clock() - start_time) / CLOCKS_PER_SEC);
    while (steps < 3) // && atomic_load(&should_run))
    {
        ret = usp_block_until_event(&ctx, &event_type, &event_buffer);
        if (ret == SEV_STEP_ERR || ret == SEV_STEP_ERR_ABORT)
        {
            printf("%11.8f:Exiting program!\n", (double)(clock() - start_time) / CLOCKS_PER_SEC);
            goto cleanup;
        }
        if (event_type == SEV_STEP_EVENT) // We have performed a step.
        {
            step_event = (sev_step_event_t *)event_buffer;

            if (step_event->counted_instructions > 0)
            {
                printf("%11.8f:Step Event: Latency: %ld, Counted instructions: %d\n",
                       (double)(clock() - start_time) / CLOCKS_PER_SEC,
                       step_event->tsc_latency,
                       step_event->counted_instructions);
                printf("%11.8f:Attempting to track all pages.\n", (double)(clock() - start_time) / CLOCKS_PER_SEC);
                if (SEV_STEP_OK != track_all_pages(&ctx, KVM_PAGE_TRACK_ACCESS))
                {
                    printf("%11.8f:Failed to track all pages.\n", (double)(clock() - start_time) / CLOCKS_PER_SEC);
                }
                printf("%11.8f:Post track_all_pages().\n", (double)(clock() - start_time) / CLOCKS_PER_SEC);
                steps++;
            }
        }
        else if (event_type == PAGE_FAULT_EVENT)
        {
            printf("%11.8f:Page fault event.\n", (double)(clock() - start_time) / CLOCKS_PER_SEC);
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
    printf("Quitting program.\n");
    return 1;
}