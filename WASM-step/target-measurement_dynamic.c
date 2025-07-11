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
    } while (elapsed_time < time);
    printf("Elapsed time: %d / %d.\n", time, time);
    atomic_exchange(&should_run, false);
    return NULL;
}

bool should_abort()
{
    // Simply check the atomic variable
    return !atomic_load(&should_run);
}

uint64_t* load_blacklist(const char *filename, size_t *count_out) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        perror("Failed to open blacklist file");
        return NULL;
    }

    // First pass: count lines
    size_t count = 0;
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        count++;
    }

    rewind(f);  // Go back to start of file

    // Allocate memory
    uint64_t *pages = malloc(count * sizeof(uint64_t));
    if (!pages) {
        perror("Memory allocation failed");
        fclose(f);
        return NULL;
    }

    // Second pass: parse lines
    size_t index = 0;
    while (fgets(line, sizeof(line), f) && index < count) {
        pages[index++] = strtoull(line, NULL, 10);
    }

    fclose(f);
    *count_out = index;
    return pages;
}

int main(int argc, char **argv)
{
    bool DEBUG_EN = false, PRINT_MEAS = false;
    bool print_help = false;

    int TRACK_MODE = KVM_PAGE_TRACK_WRITE;

    int MAX_MEAS_POINTS = 1e7;
    int MAX_MEAS_TIME = 10;
    int APIC_VAL = 0;
    uint64_t KER_MIN_HPA = 0, KER_MAX_HPA = 0;
    int opt;

    while ((opt = getopt(argc, argv, "m:t:i:r:pdh")) != -1)
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
            char *dash = strchr(optarg, '-');
            if (dash)
            {
                *dash = '\0';
                KER_MIN_HPA = strtol(optarg, NULL, 0);
                KER_MAX_HPA = strtol(dash + 1, NULL, 0);
            }
            else
            {
                fprintf(stderr, "Invalid range format. Use A-B.\n");
                return 1;
            }
        }
        break;
        default: // Includes 'h'
            print_help = true;
            break;
        }
    }
    if (KER_MAX_HPA == 0 || APIC_VAL == 0)
    {
        print_help = true;
    }
    if (print_help)
    {
        printf("Usage: %s [-t timer_value] [-r {KER_MIN_HPA}-{KER_MAX_HPA}]\n"
               "Other arguments:\n"
               "    -d  :   VM runs in debug mode. (Doesn't seem to work)\n"
               "    -p  :   Print out measurements (enabled by default with -d)\n"
               "    -i  :   Interrupt timer for max measurement time (10s default)\n"
               "    -m  :   Maximum measurement points, 1e7 default.\n",
               argv[0]);
        exit(EXIT_FAILURE);
    }
    /*
    if(CAND_DIST_THRESHOLD < PINGPONG_LEN){
        printf("Will never find the pingpong as PINGPONG_LEN > CAND_DIST_THRESHOLD.\n");
        return 0;
    }*/

    printf("Kernel page range specified at: 0x%lx - 0x%lx\n", KER_MIN_HPA, KER_MAX_HPA);

    size_t bl_page_count = 0;
    uint64_t *blacklist = load_blacklist("output/frequent_pages.txt", &bl_page_count);

    if (!blacklist) return 1;

    usp_poll_api_ctx_t ctx;
    if (SEV_STEP_ERR == usp_new_ctx(&ctx, DEBUG_EN))
    {
        printf(BRED "usp_new_ctx failed." reset " Check dmesg for more information\n");
        return 0;
    }
    printf("CTX initiated.\n");

    page_tup_candidate candidate_list[CAND_SET_SIZE] = {0};

    track_page_range(&ctx, LOWER_MEM_BOUND, KER_MIN_HPA, TRACK_MODE);
    track_page_range(&ctx, KER_MAX_HPA, UPPER_MEM_BOUND, TRACK_MODE);
    for(size_t i = 0; i < bl_page_count; i++){
        if(blacklist[i] >= KER_MIN_HPA && blacklist[i] <= KER_MAX_HPA)
            continue;
        if(SEV_STEP_ERR == untrack_page(&ctx,blacklist[i],TRACK_MODE))
            printf("Couldn't untrack page 0x%lx.\n",blacklist[i]);
    }

    usp_event_type_t event_type;
    void *event_buffer;
    //time_t start_time;

    atomic_exchange(&should_run, true);
    pthread_t t_timer;
    pthread_create(&t_timer, NULL, interrupt_thread, &MAX_MEAS_TIME);
    printf("Timer created and entering tracking loop.\n\n");

    uint64_t last_gpa = 0, curr_gpa = 0;
    while (atomic_load(&should_run))
    {
        int ret = usp_block_until_event_or_cb(&ctx, &event_type, &event_buffer, should_abort, NULL);
        if (ret == SEV_STEP_ERR_ABORT)
        {
            printf("Abort initiated! Untracking pages and exiting!\n");
            untrack_all_pages(&ctx, TRACK_MODE);
            break;
        }
        else if (event_type != PAGE_FAULT_EVENT)
        {
            printf("Unexpected event type! Untracking pages and exiting!\n");
            untrack_all_pages(&ctx, TRACK_MODE);
            break;
        }
        else
        {
            usp_page_fault_event_t *pf_event = (usp_page_fault_event_t *)event_buffer;
            curr_gpa = pf_event->faulted_gpa;
            if(PRINT_MEAS)
                printf("Pagefault Event: {GPA:0x%lx}, Mode: %d. ", curr_gpa, pf_event->fault_mode);
            if (last_gpa == 0)
            {
                last_gpa = curr_gpa;
                free_usp_event(event_type,event_buffer);
                usp_ack_event(&ctx);
                continue; // Only first occurance.
            }
            bool in_cand_list = false;
            for (int i = 0; i < CAND_SET_SIZE; i++)
            {
                if (!candidate_list[i].set)
                    continue;
                if (candidate_list[i].gpa1 == last_gpa && candidate_list[i].gpa2 == curr_gpa){ //Found a match.
                    if(PRINT_MEAS)
                        printf("In candidate list.\n");
                    in_cand_list = true;
                    candidate_list[i].cand_dist = 0;
                    candidate_list[i].occurances++;
                    if(candidate_list[i].occurances >= PINGPONG_LEN){
                        atomic_exchange(&should_run,false);
                        printf("Found a pingpong event at pages {0x%lx} - {0x%lx}!\n", last_gpa, curr_gpa);
                        untrack_all_pages(&ctx,TRACK_MODE);
                        break;
                    }
                }
                candidate_list[i].cand_dist++;
                if (candidate_list[i].cand_dist > CAND_DIST_THRESHOLD)
                { // Clear out candidates that are too inactive.
                    candidate_list[i].set = false;
                    candidate_list[i].cand_dist = 0;
                    if(PRINT_MEAS)
                        printf("Cleared out a candidate in the list.\n");
                }
            }
            if(!in_cand_list){
                for (int i = 0; i < CAND_SET_SIZE; i++){
                    if (!candidate_list[i].set){
                        candidate_list[i].gpa1 = last_gpa;
                        candidate_list[i].gpa2 = curr_gpa;
                        candidate_list[i].occurances = 1;
                        candidate_list[i].set = true;
                        break;
                    }
                }
            }
            if(last_gpa < KER_MIN_HPA || last_gpa > KER_MAX_HPA)
                track_page(&ctx,last_gpa,TRACK_MODE);
            last_gpa = curr_gpa;
            free_usp_event(event_type,event_buffer);
            usp_ack_event(&ctx);
        }
    }

    if (SEV_STEP_ERR == usp_close_ctx(&ctx))
    {
        printf(BRED "usp_close_ctx failed." reset " Check dmesg for more information\n");
        return 0;
    }

    free(blacklist);
    pthread_cancel(t_timer);
    

    return 1;
}