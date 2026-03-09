#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

#include <getopt.h>
#include <fcntl.h>
#include <unistd.h>

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

    
static write_queue_t wq;
static FILE *fptr = NULL;

const char start_string[] = "AEEEWEEWWEEWEEWEWWWWWWWEEAAEEEAAEAEAAEWWEEEAEE";
const int32_t start_string_len = sizeof(start_string)-1;
const char end_string[] = "EWAEEEEEEEAAAE";
const int32_t end_string_len = sizeof(end_string)-1;

// ---------- Writer thread ----------
void *t_writer(void *arg __attribute__((unused)))
{
    time_t last_flush = time(NULL);
    while (true) {
        pthread_mutex_lock(&wq.mtx);
        while (wq.head == wq.tail && !wq.done)
            pthread_cond_wait(&wq.cv_nonempty, &wq.mtx);

        if (wq.head == wq.tail && wq.done) {
            pthread_mutex_unlock(&wq.mtx);
            break;
        }

        char *line = wq.lines[wq.tail];
        wq.tail = (wq.tail + 1) % WRITE_QUEUE_SIZE;
        pthread_cond_signal(&wq.cv_nonfull);
        pthread_mutex_unlock(&wq.mtx);

        if (line) {
            fwrite(line, 1, strlen(line), fptr);
            free(line);
        }

        time_t now = time(NULL);
        if (difftime(now, last_flush) >= FLUSH_INTERVAL_SEC) {
            fflush(fptr);
            last_flush = now;
        }
    }
    fflush(fptr);
    fclose(fptr);
    printf("Writer thread exited cleanly.\n");
    return NULL;
}

// ---------- Writer queue setup ----------
void init_writer_queue()
{
    memset(&wq, 0, sizeof(wq));
    pthread_mutex_init(&wq.mtx, NULL);
    pthread_cond_init(&wq.cv_nonempty, NULL);
    pthread_cond_init(&wq.cv_nonfull, NULL);
    atomic_store(&wq.done, false);
}

void enqueue_line(const char *line)
{
    // copy to heap (since line is temporary)
    char *copy = strdup(line);
    if (!copy) return;

    pthread_mutex_lock(&wq.mtx);
    size_t next_head = (wq.head + 1) % WRITE_QUEUE_SIZE;
    while (next_head == wq.tail)
        pthread_cond_wait(&wq.cv_nonfull, &wq.mtx); // wait if queue full

    wq.lines[wq.head] = copy;
    wq.head = next_head;
    pthread_cond_signal(&wq.cv_nonempty);
    pthread_mutex_unlock(&wq.mtx);
}

void stop_writer()
{
    pthread_mutex_lock(&wq.mtx);
    atomic_store(&wq.done, true);
    pthread_cond_signal(&wq.cv_nonempty);
    pthread_mutex_unlock(&wq.mtx);
}


void write_line(const char *line)
{
    enqueue_line(line);
}

int main(int argc, char **argv)
{
    bool DEBUG_EN = false;
    bool print_help = false;
    uint64_t MAX_MEAS_POINTS = 5e9; // 1e9 ~50GB of data
    int APIC_VAL = 0;
    uint64_t ATTACK_PAGE_HPA[2] = {0};

    char note[100] = {'\0'};
    char line[1024];

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
        case 'd':
            DEBUG_EN = true;
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
               "    -m  :   Maximum measurement points, 1e7 default.\n",
               argv[0]);
        exit(EXIT_FAILURE);
    }

    char fileName[FILENAME_CHAR_LIM] = "output/";
    for (int i = 0; note[i] != '\0'; i++)
    {
        if (i + 7 >= FILENAME_CHAR_LIM)
        {
            printf("Too long filename: exiting.\n");
            exit(EXIT_FAILURE);
        }
        fileName[i + 7] = note[i];
    }

    printf("Attack page specified at: 0x%lx\n", ATTACK_PAGE_HPA[0]);
    printf("Monitor page specified at: 0x%lx\n", ATTACK_PAGE_HPA[1]);

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
    printf("Attack page accessed, waiting for monitor page access to start measurement...\n");

    TRACK_PAGE(ATTACK_PAGE_HPA[1], KVM_PAGE_TRACK_WRITE);
    FREE_AND_ACK_EV();
    printf("Waiting for pagefault event at GPA: 0x%lx\n", ATTACK_PAGE_HPA[1]);
    ret = usp_block_until_event(&ctx, &event_type, &event_buffer);
    if (ret == SEV_STEP_ERR_ABORT || ret == SEV_STEP_ERR || event_type != PAGE_FAULT_EVENT)
    {
        printf("Exiting program!\n");
        event_buffer = NULL;
        goto cleanup;
    }
    // Used for page fault tracking.
    uint64_t curr_ex_gpa = 0;
    uint64_t pf_arr[PF_LIMIT] = {0};
    char last_pF_mode;
    int32_t pf_count = 0;

    uint64_t steps = 0;

    fptr = fopen(fileName, "w");
    if (!fptr) {
        perror("Failed to open output file");
        exit(EXIT_FAILURE);
    }
    init_writer_queue();
    pthread_t writer_thread;
    pthread_create(&writer_thread, NULL, t_writer, NULL);

    fprintf(fptr, "Steps,Latency,Execution Page,Last PF Mode,PF num,Last PF,Counted Instructions,Timestamp[us]");
    track_all_pages(&ctx, KVM_PAGE_TRACK_ACCESS);
    enable_single_stepping(&ctx, APIC_VAL, NULL, 0);

    uint64_t min_steps = 10;
    time_t meas_start = time(NULL);
    time_t start_time = clock();
    printf("Proceeding to measurement.\n");
    FREE_AND_ACK_EV();
    while (steps < MAX_MEAS_POINTS) // && atomic_load(&should_run))
    {
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
                // printf("Weird step event, step %lu.\n", steps);
                // FREE_AND_ACK_EV();
                // goto cleanup;
            }
            else if (event_steps > 0 && event_steps < MULTISTEP_LIMIT)
            {
                steps += event_steps;
                snprintf(line, sizeof(line), "\n%lu,%ld,0x%lx,%c,%d,0x%lx,%d,%ld",
                         steps,
                         step_event->tsc_latency,
                         curr_ex_gpa,
                         last_pF_mode,
                         pf_count,
                         pf_arr[pf_count - 1],
                         event_steps,
                         clock() - start_time);
                write_line(line);
                for (int i = pf_count - 1; i >= 0; i--)
                {
                    TRACK_PAGE(pf_arr[i], KVM_PAGE_TRACK_ACCESS)
                }
                pf_count = 0;
            }
            else if (event_steps >= MULTISTEP_LIMIT)
            {
                printf("Overstepped step event, step %lu with %d instructions.\n", steps, event_steps);
                FREE_AND_ACK_EV();
                break;
            }
        }
        else if (event_type == PAGE_FAULT_EVENT)
        {
            pf_event = (usp_page_fault_event_t *)event_buffer;
            if (pf_event->faulted_gpa == ATTACK_PAGE_HPA[0] && steps > min_steps)
            {
                printf("Attack page accessed at: 0x%lx\n", pf_event->faulted_gpa);
                FREE_AND_ACK_EV(); // Acknowledge the page fault event.
                break;             // Exit the loop if the attack page is accessed.
            }

            pf_arr[pf_count] = pf_event->faulted_gpa;
            if (pf_event->pf_exec)
            {
                last_pF_mode = 'E';
                curr_ex_gpa = pf_event->faulted_gpa;
            }
            else if (pf_event->pf_write)
            {
                last_pF_mode = 'W';
            }
            else
            {
                last_pF_mode = 'A';
            }
            pf_count++;

        }
        FREE_AND_ACK_EV();
    }

    FILE *log_file = fopen(LOG_FILE_PATH, "a");
    if (!log_file)
    {
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

    stop_writer();
    pthread_join(writer_thread, NULL);
    printf("Quitting program.\n");
    return 1;
}
