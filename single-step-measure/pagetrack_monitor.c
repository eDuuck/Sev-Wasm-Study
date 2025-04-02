#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <limits.h>
#include <getopt.h>
#include <time.h>
#include <inttypes.h>

#include "../sev-step-lib/ansi-color-codes.h"
#include "../sev-step-lib/sev_step_api.h"

#include "common_structs.h"


void *page_track_thread(single_step_measure_args *args)
{
    usp_poll_api_ctx_t ctx;

    bool api_open = false;
    int res = HOST_CLIENT_SUCCESS;

    printf("%sInitializing API...\n", args->format_prefix);
    if (SEV_STEP_ERR == usp_new_ctx(&ctx, args->debug_enabled))
    {
        printf("%s" BRED "usp_new_ctx failed." reset " Check dmesg for more information\n", args->format_prefix);
        return HOST_CLIENT_ERROR;
    }
    printf("%sAPI initialization done!\n", args->format_prefix);
    api_open = true;

    // Try tracking all pages to infer access pattern.
    // modes = KVM_PAGE_TRACK_ACCESS or KVM_PAGE_TRACK_WRITE or KVM_PAGE_TRACK_EXEC
    int track_mode = KVM_PAGE_TRACK_EXEC;

    printf("%sTracking all gpa with mode %s\n", args->format_prefix, tracking_mode_to_string(track_mode));
    track_all_pages(&ctx, track_mode);

    usp_event_type_t event_type;
    void *event_buffer;
    int input;
    // Playing around to see if we can print out the measured pages.
    for (int event_idx = 0; event_idx < PAGE_TRACKS; event_idx++)
    {
        usp_block_until_event(&ctx, &event_type, &event_buffer);
        if (event_type != PAGE_FAULT_EVENT)
        {
            printf("Didn't get a pagefault, event type is %d\n", (int)event_type);
            goto cleanup;
        }
        printf("%sGot a pagefault!\n", args->format_prefix);
        usp_page_fault_event_t *pf_event = (usp_page_fault_event_t *)event_buffer;
        printf("%sPagefault Event: {GPA:0x%lx}\n", args->format_prefix, pf_event->faulted_gpa);

        // Track all pages except the one we just encountered so VM can continue execution.
        // track_all_pages(&ctx, track_mode);
        untrack_page(&ctx, pf_event->faulted_gpa, track_mode);

        // Waiting for user to allow next page read.
        if (input != 10)
        {
            printf("%sPaused until user input any number. Enter \"10\" to proceed with execution of rest %d events.", args->format_prefix, pageTracks - event_idx);
            scanf("%d", &input);
        }
        // Clearing event to resume VM execution.
        printf("%sSending ack for event_idx %d\n", args->format_prefix, event_idx);
        usp_ack_event(&ctx);
        free_usp_event(event_type, event_buffer);
    }

cleanup:
    if (api_open)
    {
        if (SEV_STEP_ERR == usp_close_ctx(&ctx))
        {
            printf("%s" BRED "usp_close_ctx failed." reset " Check dmesg for more information\n", args->format_prefix);
        }
        else
        {
            printf("%sAPI closed!\n", args->format_prefix);
        }
    }
    return res;
}



int main() {
    pthread_t worker;
    pthread_create(&worker, NULL, ping_ponger_thread, NULL);

    sleep(1); // Simulate delay before starting
    pthread_mutex_lock(&mutex);
    start_flag = 1;
    pthread_cond_signal(&start_cond);
    pthread_mutex_unlock(&mutex);

    sleep(2); // Let it run for a bit
    pthread_mutex_lock(&mutex);
    stop_flag = 1;
    pthread_mutex_unlock(&mutex);
    
    pthread_join(worker, NULL);
    return 0;
}