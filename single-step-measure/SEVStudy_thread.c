#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>

#include <time.h>

#include <stdatomic.h>

#include "SEVStudy_thread.h"
#include "server/vm_server.h"

#include "../sev-step-lib/ansi-color-codes.h"
#include "../sev-step-lib/sev_step_api.h"

#include "errorCodes.h"
#include "common_structs.h"

#include <sys/stat.h>

static usp_poll_api_ctx_t ctx;
bool api_open, single_stepping_enabled, gpa_set;
single_step_measure_args *args;
usp_event_type_t event_type;
void *event_buffer;

uint64_t gpa1, gpa2;

static atomic_bool running = 0;
static atomic_bool measure_active = 0;

void *mock_thread()
{
    FILE *fptr;
    int *latency_vals = malloc(sizeof(int) * MAX_STEP_AMOUNT);
    char *counted_inst = malloc(sizeof(char) * MAX_STEP_AMOUNT);

    int elapsedEvents = 0;

    struct timespec ts;
    ts.tv_nsec = 1;

    while (!atomic_load(&running))
        ;

    while (elapsedEvents < MAX_STEP_AMOUNT && atomic_load(&running))
    {

        counted_inst[elapsedEvents] = elapsedEvents;
        latency_vals[elapsedEvents] = elapsedEvents;

        nanosleep(&ts, NULL);
        elapsedEvents++;
    }

    if (elapsedEvents > MAX_STEP_AMOUNT)
        printf("Maximum measurements of %d exceeded. Finishing.\n", MAX_STEP_AMOUNT);

    //
    // while (atomic_load(&running));
    fptr = fopen("Mock_file", "w");
    for (int i = 0; i < elapsedEvents; i++)
    {
        fprintf(fptr, "%d,%d:", latency_vals[i], counted_inst[i]);
        printf("Wrote value %d / %d \n", i + 1, elapsedEvents);
    }
    free(latency_vals);
    free(counted_inst);
    fclose(fptr);
    return NULL;
}

void start_meas()
{
    atomic_exchange(&running, 1);
}

void stop_meas()
{
    atomic_exchange(&running, 0);
}

bool should_abort_cb()
{
    // Simply check the atomic variable
    return (atomic_load(&running) == 0);
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
    api_open = true;
    return 1; /*
     // Weird code needed so VM doesn't freeze.
     track_all_pages(&ctx, KVM_PAGE_TRACK_EXEC);
     untrack_all_pages(&ctx, KVM_PAGE_TRACK_EXEC);
     //usp_ack_event(&ctx);
     usp_close_ctx(&ctx);

     if (SEV_STEP_ERR == usp_new_ctx(&ctx, args->debug_enabled))
     {
         printf("%s" BRED "usp_new_ctx failed." reset " Check dmesg for more information\n", args->format_prefix);
         return HOST_CLIENT_ERROR;
     }
     api_open = true;

     printf("%sAPI initialization done!\n", args->format_prefix);
     return 1;*/
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
            api_open = false;
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

bool is_measure_active()
{
    return atomic_load(&measure_active);
}

void *meas_thread()
{
    // Setting up data structures for measurement (Using int and char to reduce size)
    int *latency_vals = malloc(sizeof(int) * MAX_STEP_AMOUNT);
    char *counted_inst = malloc(sizeof(char) * MAX_STEP_AMOUNT);

    int elapsedEvents = 0;

    // bool encountered_multistep = false;

    while (!atomic_load(&running)); // Wait here until main thread inform to start measurement.
    atomic_exchange(&measure_active, true);
    while (elapsedEvents < MAX_STEP_AMOUNT && atomic_load(&running))
    {
        if (args->print_meas)
            printf("%sWaiting next event, event_idx=%d\n", args->format_prefix, elapsedEvents);
        int ret = usp_block_until_event_or_cb(&ctx, &event_type, &event_buffer, should_abort_cb, NULL);
        if (ret == SEV_STEP_ERR_ABORT)
            break;
        if (ret == SEV_STEP_ERR)
        {
            printf("%susp_block_until_event" BRED " FAILED\n" reset, args->format_prefix);
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
    atomic_exchange(&measure_active, false);
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
void set_gpas(uint64_t i_gpa1, uint64_t i_gpa2)
{
    gpa1 = i_gpa1;
    gpa2 = i_gpa2;
    gpa_set = true;
}

void *track_pingpong()
{
    int track_mode = KVM_PAGE_TRACK_WRITE;

    if (!gpa_set)
        printf("%sgpa1 and gpa2 not set, this will probably cause errors.\n", args->format_prefix);

    int tracked_pages = 0;

    while (!atomic_load(&running))
        ;

    track_page(&ctx, gpa1, track_mode);
    // track_page(&ctx, gpa2, track_mode);
    printf("%sReady to track pingpong pages.\n", args->format_prefix);
    while (tracked_pages < PAGE_TRACKS && atomic_load(&running))
    {
        int ret = usp_block_until_event_or_cb(&ctx, &event_type, &event_buffer, should_abort_cb, NULL);
        if (ret == SEV_STEP_ERR_ABORT)
            break;
        if (event_type != PAGE_FAULT_EVENT)
        {
            printf("Didn't get a pagefault, event type is %d\n", (int)event_type);
            break;
        }
        printf("%sGot a pagefault!\n", args->format_prefix);
        usp_page_fault_event_t *pf_event = (usp_page_fault_event_t *)event_buffer;
        printf("%sPagefault Event: {GPA:0x%lx}\n", args->format_prefix, pf_event->faulted_gpa);

        // Track all pages except the one we just encountered so VM can continue execution.
        // untrack_page(&ctx, pf_event->faulted_gpa, track_mode);
        if (pf_event->faulted_gpa == gpa1)
        {
            track_page(&ctx, gpa2, track_mode);
        }
        else
        {
            track_page(&ctx, gpa1, track_mode);
        }

        tracked_pages++;

        // Clearing event to resume VM execution.
        // printf("%sSending ack for event_idx %d\n", args->format_prefix, event_idx);
        usp_ack_event(&ctx);
        free_usp_event(event_type, event_buffer);
    }
    printf("%sThread done.\n", args->format_prefix);
    return NULL;
}

void *inside_pingpong_measure()
{
    int track_mode = KVM_PAGE_TRACK_WRITE;
    int *latency_vals = malloc(sizeof(int) * MAX_STEP_AMOUNT);
    int *counted_inst = malloc(sizeof(int) * MAX_STEP_AMOUNT);

    if (!gpa_set)
        printf("%sgpa1 and gpa2 not set, this will probably cause errors.\n", args->format_prefix);

    int tracked_pages = 0;
    bool pingpong_done = false;

    while (!atomic_load(&running));

    track_page(&ctx, gpa1, track_mode);
    printf("%sReady to track pingpong pages.\n", args->format_prefix);
    // First we check for the specified pingpong accesses to sync execution.
    while (tracked_pages < PAGE_TRACKS && atomic_load(&running) && !pingpong_done)
    {
        int ret = usp_block_until_event_or_cb(&ctx, &event_type, &event_buffer, should_abort_cb, NULL);
        if (ret == SEV_STEP_ERR_ABORT)
            break; // So we can stop the thread.
        if (event_type != PAGE_FAULT_EVENT)
        {
            printf("%sDidn't get a pagefault, event type is %d\n", args->format_prefix, (int)event_type);
            break;
        }
        tracked_pages++;
        
        printf("%sGot a pagefault!\n", args->format_prefix);
        usp_page_fault_event_t *pf_event = (usp_page_fault_event_t *)event_buffer;
        printf("%sPagefault Event: {GPA:0x%lx}\n", args->format_prefix, pf_event->faulted_gpa);

        if (tracked_pages == PINGPONG_LEN * 2)
        {
            printf("%sPingpong should finish. Tracking all pages\n", args->format_prefix);

            track_page(&ctx, gpa1, track_mode); //IMPORTANT HERE TO RUN track_page -> track_all_pages -> enable_single_stepping(NULL).
            track_all_pages(&ctx, KVM_PAGE_TRACK_EXEC);
            //track_page(&ctx, gpa2, track_mode);
            printf("%sEnabling single stepping with apic timer %d. \n \n", args->format_prefix,args->apic_timer);
            enable_single_stepping(&ctx,args->apic_timer,NULL,0);
            pingpong_done = true;
        }

        if (pf_event->faulted_gpa == gpa1)
        {
            track_page(&ctx, gpa2, track_mode);
        }
        else
        {
            track_page(&ctx, gpa1, track_mode);
        }

        usp_ack_event(&ctx);
        free_usp_event(event_type, event_buffer);
    }
    
    // Now we're out of the pingpong loop, next is the add or mul. So we measure single steps until a pagehit in gpa1.
    int elapsedEvents = 0;
    int zeroSteps = 0;


    volatile bool stepping_done = false;
    atomic_exchange(&measure_active, true);
    while (elapsedEvents < MAX_STEP_AMOUNT && atomic_load(&running) && !stepping_done)
    {
        int ret = usp_block_until_event_or_cb(&ctx, &event_type, &event_buffer, should_abort_cb, NULL);
        if (ret == SEV_STEP_ERR_ABORT)
            break;
        else if (ret == SEV_STEP_ERR)
        {
            printf("%susp_block_until_event" BRED " FAILED\n" reset, args->format_prefix);
            return NULL;
        }
        if (event_type == PAGE_FAULT_EVENT)
        { // We're inside some new page.
            usp_page_fault_event_t *pf_event = (usp_page_fault_event_t *)event_buffer;
            uint64_t pf_gpa = pf_event->faulted_gpa;
            printf("%sEntered a new page at GPA: 0x%lx.\n", args->format_prefix, pf_gpa);
            if (pf_gpa == gpa1 || pf_gpa == gpa2)
            {
                printf("%sOne of the pingpong pages, finishing....\n\n", args->format_prefix);
                untrack_all_pages(&ctx, KVM_PAGE_TRACK_EXEC);
                untrack_all_pages(&ctx, KVM_PAGE_TRACK_ACCESS);
                untrack_all_pages(&ctx, KVM_PAGE_TRACK_WRITE);
                disable_single_stepping(&ctx);
                stepping_done = true;
            }
            else
            {  
                //Not the pingpong page, therefor victim page!
                track_page(&ctx, gpa1, KVM_PAGE_TRACK_WRITE);
                track_all_pages(&ctx, KVM_PAGE_TRACK_EXEC);
                enable_single_stepping(&ctx,args->apic_timer,&pf_gpa,1); //Since enable stepping resets access bit we don't need to untrack pf_gpa.
                printf("%sNot one of the pingpong pages.\n", args->format_prefix);
            }
        }
        else if (event_type == SEV_STEP_EVENT)
        {
            sev_step_event_t *step_event = (sev_step_event_t *)event_buffer;
            if (args->print_meas){
                print_single_step_event(args->format_prefix, step_event);
            }            
            if(step_event->counted_instructions > 0){
                elapsedEvents++;
                counted_inst[elapsedEvents-1] = step_event->counted_instructions;
                latency_vals[elapsedEvents-1] = (int)step_event->tsc_latency;
            }else{
                zeroSteps++;
                if(zeroSteps >= MAX_ZERO_STEPS){
                    printf("%sToo many single steps occured, %d non-zero step events with %d zero steps.\n",args->format_prefix,elapsedEvents,zeroSteps);
                    stepping_done = true;
                }
            }
        }else{
            printf("%sUnkown event?",args->format_prefix);
        }
        usp_ack_event(&ctx);
        free_usp_event(event_type, event_buffer);
    }
    atomic_exchange(&measure_active, false);

    if (elapsedEvents >= MAX_STEP_AMOUNT)
        printf("%sMaximum measurements exceeded. Finishing.", args->format_prefix);
    
    time_t t;
    char fileName[40];
    struct tm *tmp;
    time(&t);
    tmp = localtime(&t);
    strftime(fileName,sizeof(fileName),"output/Measurement_%m%d_%H%M%S.csv", tmp);

    FILE *fptr;

    fptr = fopen(fileName, "w");

    fprintf(fptr, "#This measurement was done on function %s\n",args->call_fun);
    fprintf(fptr, "Step_event;Latency Value;Counted Steps\n");
    for (int i = 0; i < elapsedEvents; i++)
    {
        fprintf(fptr, "%d;%d;%d\n", i,latency_vals[i], counted_inst[i]);
    }
    fclose(fptr);

    close_CTX();

    free(latency_vals);
    free(counted_inst);
    printf("%sThread done.\n", args->format_prefix);
    return NULL;
}

