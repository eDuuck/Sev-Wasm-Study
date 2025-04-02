#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>

#include <stdatomic.h>

#include "SEVStudy_thread.h"

#include "../sev-step-lib/ansi-color-codes.h"
#include "../sev-step-lib/sev_step_api.h"

#include "errorCodes.h"
#include "common_structs.h"

usp_poll_api_ctx_t ctx;
bool api_open, single_stepping_enabled;
single_step_measure_args *args;

static atomic_bool running = 0;
static atomic_bool measure_active = 0;

void *mock_thread(){
    FILE *fptr;
    int *latency_vals = malloc(sizeof(int)* MAX_STEP_AMOUNT);
    char *counted_inst = malloc(sizeof(char)* MAX_STEP_AMOUNT);

    int elapsedEvents = 0;

    struct timespec ts;
    ts.tv_nsec = 1;

    while (!atomic_load(&running));

    while (elapsedEvents < MAX_STEP_AMOUNT && atomic_load(&running)){

        counted_inst[elapsedEvents] = elapsedEvents;
        latency_vals[elapsedEvents] = elapsedEvents;
        
        nanosleep(&ts, NULL);
        elapsedEvents++;
    }
    
    if (elapsedEvents > MAX_STEP_AMOUNT)
        printf("Maximum measurements of %d exceeded. Finishing.\n", MAX_STEP_AMOUNT);

    //
    //while (atomic_load(&running));
    fptr = fopen("Mock_file", "w");
    for (int i = 0; i < elapsedEvents; i++)
    {
        fprintf(fptr, "%d,%d:", latency_vals[i], counted_inst[i]);
        printf("Wrote value %d / %d \n",i+1,elapsedEvents);
    }
    fclose(fptr);
    return NULL;
}

void start_meas(){
    atomic_exchange(&running, 1);
}

void stop_meas(){
    atomic_exchange(&running, 0);
}

int init_CTX(single_step_measure_args *arguments)
{
    args = arguments;
    printf("%sInitializing API...\n", args->format_prefix);
    if (SEV_STEP_ERR == usp_new_ctx(&ctx, args->debug_enabled))
    {
        printf("%s" BRED "usp_new_ctx failed." reset " Check dmesg for more information\n", args->format_prefix);
        return HOST_CLIENT_ERROR;
    }

    // Weird code needed so VM doesn't freeze.
    track_all_pages(&ctx, KVM_PAGE_TRACK_EXEC);
    untrack_all_pages(&ctx, KVM_PAGE_TRACK_EXEC);
    usp_ack_event(&ctx);
    usp_close_ctx(&ctx);

    if (SEV_STEP_ERR == usp_new_ctx(&ctx, args->debug_enabled))
    {
        printf("%s" BRED "usp_new_ctx failed." reset " Check dmesg for more information\n", args->format_prefix);
        return HOST_CLIENT_ERROR;
    }
    api_open = true;

    printf("%sAPI initialization done!\n", args->format_prefix);
    return 1;
}

int close_CTX()
{
    if (api_open)
    {
        if (SEV_STEP_ERR == usp_close_ctx(&ctx))
        {
            printf("%s" BRED "usp_close_ctx failed." reset " Check dmesg for more information\n", args->format_prefix);
            return 0;
        }
        else
        {
            printf("%sAPI closed!\n", args->format_prefix);
            return 1;
        }
    }
    else
    {
        printf("%sAPI already closed!\n", args->format_prefix);
        return 1;
    }
}

int en_single_step()
{
    printf("%sEnabling  single stepping\n", args->format_prefix);
    if (SEV_STEP_OK != enable_single_stepping(&ctx, args->apic_timer, NULL, 0))
    {
        printf("%sfailed to enable single stepping\n", args->format_prefix);
        return HOST_CLIENT_ERROR;
    }
    single_stepping_enabled = true;
    return 1;
}

bool is_measure_active(){
    return atomic_load(&measure_active);
}

void *meas_thread(){
    usp_event_type_t event_type;
    void *event_buffer;

    // Setting up data structures for measurement (Using int and char to reduce size)
    int *latency_vals = malloc(sizeof(int)* MAX_STEP_AMOUNT);
    char *counted_inst = malloc(sizeof(char)* MAX_STEP_AMOUNT);

    int elapsedEvents = 0;

    bool encountered_multistep = false;

    while(!atomic_load(&running)); //Wait here until main thread inform to start measurement.
    atomic_exchange(&measure_active,true);
    while (elapsedEvents < MAX_STEP_AMOUNT && atomic_load(&running))
    {
        if (args->print_meas)
            ("%sWaiting next event, event_idx=%d\n", args->format_prefix, elapsedEvents);
        if (SEV_STEP_ERR == usp_block_until_event(&ctx, &event_type, &event_buffer))
        {
            printf("%susp_block_until_event" BRED " FAILED\n" reset, args->format_prefix);
            return NULL;
        }

        if (event_type != SEV_STEP_EVENT)
        {
            printf("%sunexpected event type\n", args->format_prefix);
            return NULL;
        }

        sev_step_event_t *step_event = (sev_step_event_t *)event_buffer;

        if (args->print_meas)
        {
            print_single_step_event(args->format_prefix, step_event);
            switch (step_event->counted_instructions)
            {
            case 0:
                printf("%sZero steps\n", args->format_prefix);
                break;
            case 1:
                printf("%sSingle step\n", args->format_prefix);
                break;
            default:
                printf("%sMulti step\n", args->format_prefix);
                break;
            }
        }

        counted_inst[elapsedEvents] = step_event->counted_instructions;
        latency_vals[elapsedEvents] = (int)step_event->tsc_latency;
        if (args->print_meas)
            printf("%sSending ack for event_idx %d\n", args->format_prefix, elapsedEvents);
        elapsedEvents++;
        usp_ack_event(&ctx);
        free_usp_event(event_type, event_buffer);
        // event_buffer = NULL;
    }
    atomic_exchange(&measure_active,false);
    if (elapsedEvents > MAX_STEP_AMOUNT)
        printf("%sMaximum measurements exceeded. Finishing.", args->format_prefix);

    FILE *fptr;
    fptr = fopen("Latency_Measurements", "w");
    for (int i = 0; i < elapsedEvents; i++)
    {
        fprintf(fptr, "%d,%d:", latency_vals[i], counted_inst[i]);
    }
    fclose(fptr);

    return NULL;
}

void *page_track()
{
    // Try tracking all pages to infer access pattern.
    // modes = KVM_PAGE_TRACK_ACCESS or KVM_PAGE_TRACK_WRITE or KVM_PAGE_TRACK_EXEC
    int track_mode = KVM_PAGE_TRACK_WRITE;

    printf("%sTracking all gpa with mode %s\n", args->format_prefix, tracking_mode_to_string(track_mode));
    track_all_pages(&ctx, track_mode);

    usp_event_type_t event_type;
    void *event_buffer;
    int tracked_pages = 0;

    while(!atomic_load(&running)); //Wait here until main thread inform to start measurement.
    atomic_exchange(&measure_active,true);
    // Playing around to see if we can print out the measured pages.
    while (tracked_pages < PAGE_TRACKS && atomic_load(&running))
    {
        usp_block_until_event(&ctx, &event_type, &event_buffer);
        if (event_type != PAGE_FAULT_EVENT)
        {
            printf("Didn't get a pagefault, event type is %d\n", (int)event_type);
            break;
        }
        printf("%sGot a pagefault!\n", args->format_prefix);
        usp_page_fault_event_t *pf_event = (usp_page_fault_event_t *)event_buffer;
        printf("%sPagefault Event: {GPA:0x%lx}\n", args->format_prefix, pf_event->faulted_gpa);

        // Track all pages except the one we just encountered so VM can continue execution.
        track_all_pages(&ctx, track_mode);
        untrack_page(&ctx, pf_event->faulted_gpa, track_mode);

        tracked_pages++;

        // Clearing event to resume VM execution.
        //printf("%sSending ack for event_idx %d\n", args->format_prefix, event_idx);
        usp_ack_event(&ctx);
        free_usp_event(event_type, event_buffer);
    }
    atomic_exchange(&measure_active,false);
    untrack_all_pages(&ctx,track_mode);
    return NULL;
}
