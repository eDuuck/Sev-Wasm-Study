#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <limits.h>
#include <getopt.h>

#include "../sev-step-lib/ansi-color-codes.h"
#include "../sev-step-lib/sev_step_api.h"
#include "errorCodes.h"

#define stepAmount 10000
/* CODING DAIRY
usp = user space polling
First the API needs to be initialized, this is done through the usp_new_ctx function.
Input parsing is done, moving single-stepping into a function instead of staying in main funciton.
TODO: Pause the VM to ensure functionality.
*/
typedef struct
{
    int apic_timer;
    char *format_prefix;
    bool debug_enabled;
} single_step_measure_args;

void find_timer_value()
{
    printf("Not implemented yet.\n");
    exit(EXIT_SUCCESS);
}

int testfunc(single_step_measure_args *args)
{
    printf("Running test function\n");
    usp_poll_api_ctx_t ctx;
    bool api_open;
    printf("%sInitializing API...\n", args->format_prefix);
    if (SEV_STEP_ERR == usp_new_ctx(&ctx, args->debug_enabled))
    {
        printf("%s" BRED "usp_new_ctx failed." reset " Check dmesg for more information\n", args->format_prefix);
        return HOST_CLIENT_ERROR;
    }
    api_open = true;
    printf("%sAPI initialization done!\n", args->format_prefix);
    char input;
    int* res;
    usp_event_type_t event_type;
    void *event_buffer;
    do
    {
        printf("Press 'q' to quit\n");
        scanf("%c", &input);
        printf("you input %c\n", input);
        switch (input)
        {
        case 'i':
            if (SEV_STEP_ERR == usp_block_until_event(&ctx, &event_type, &event_buffer))
            {
                printf("%susp_block_until_event" BRED " FAILED\n" reset, args->format_prefix);
                goto cleanup;
            }
            break;
        case 't':
            printf("%d",track_all_pages(&ctx,KVM_PAGE_TRACK_ACCESS));
            break;
        case 'p':
            usp_poll_event(&ctx,res,&event_type,&event_buffer);
            break;
        default:
            printf("\n");
            break;
        }

    } while (input != 'q');

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
    return 1;
}

int single_step_measure(single_step_measure_args *args)
{
    usp_poll_api_ctx_t ctx;

    bool single_stepping_enabled = false;
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
    printf("%sEnabling  single stepping\n", args->format_prefix);

    if (SEV_STEP_OK != enable_single_stepping(&ctx, args->apic_timer, NULL, 0))
    {
        printf("%sfailed to enable single stepping\n", args->format_prefix);
        res = HOST_CLIENT_ERROR;
        goto cleanup;
    }
    single_stepping_enabled = true;
    usp_event_type_t event_type;
    void *event_buffer;

    //
    // Main Event Loop. Simply log @args->want_steps many steps before disabling single stepping again
    //

    bool encountered_multistep = false;

    for (int current_event_idx = 0; current_event_idx < stepAmount; current_event_idx++)
    {
        printf("%sWaiting next event, event_idx=%d\n", args->format_prefix, current_event_idx);
        if (SEV_STEP_ERR == usp_block_until_event(&ctx, &event_type, &event_buffer))
        {
            printf("%susp_block_until_event" BRED " FAILED\n" reset, args->format_prefix);
            res = HOST_CLIENT_ERROR;
            goto cleanup;
        }

        if (event_type != SEV_STEP_EVENT)
        {
            printf("%sunexpected event type\n", args->format_prefix);
            res = HOST_CLIENT_ERROR;
            goto cleanup;
        }

        sev_step_event_t *step_event = (sev_step_event_t *)event_buffer;
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
            if (!encountered_multistep)
            {
                encountered_multistep = true;
            }
            break;
        }

        printf("%sSending ack for event_idx %d\n", args->format_prefix, current_event_idx);
        usp_ack_event(&ctx);
        free_usp_event(event_type, event_buffer);
    }

    printf("%sDisabling single stepping\n", args->format_prefix);
    if (SEV_STEP_OK != disable_single_stepping(&ctx))
    {
        printf("%sfailed to disable single stepping\n", args->format_prefix);
        res = HOST_CLIENT_ERROR;
        goto cleanup;
    }
    single_stepping_enabled = false;
    printf("%sClosing API...\n", args->format_prefix);
    if (SEV_STEP_ERR == usp_close_ctx(&ctx))
    {
        printf("%s" BRED "usp_close_ctx failed." reset " Check dmesg for more information\n", args->format_prefix);
        res = HOST_CLIENT_ERROR;
        goto cleanup;
    }
    printf("%sAPI closed!\n", args->format_prefix);
    api_open = false;

cleanup:
    if (single_stepping_enabled)
    {
        printf("%sDisabling single stepping\n", args->format_prefix);
        if (SEV_STEP_OK != disable_single_stepping(&ctx))
        {
            printf("%sfailed to disable single stepping\n", args->format_prefix);
            res = HOST_CLIENT_ERROR;
            goto cleanup;
        }
    }
    if (api_open)
    {
        if (SEV_STEP_ERR == usp_close_ctx(&ctx))
        {
            printf("%s" BRED "usp_close_ctx failed." reset " Check dmesg for more information\n", args->format_prefix);
            res = HOST_CLIENT_ERROR;
        }
        else
        {
            printf("%sAPI closed!\n", args->format_prefix);
        }
    }
    return 1;
}

int main(int argc, char **argv)
{
    char *format_prefix = "   ";
    int apic_timer;
    int opt;
    bool debug_enabled = false;
    bool run_testfunc = false;

    while ((opt = getopt(argc, argv, "t:dfhx")) != -1)
    {
        switch (opt)
        {
        case 'd':
            debug_enabled = true;
            break;
        case 't':
            apic_timer = atoi(optarg);
            if (apic_timer == 0)
            {
                printf("Invalid timer value\n");
                exit(EXIT_FAILURE);
            }
            break;
        case 'h':
            printf("Usage: %s [-t timer_value]\n", argv[0]);
            exit(EXIT_SUCCESS);
        case 'f':
            find_timer_value();
        case 'x':
            run_testfunc = true;
            break;
        default:
            printf("Usage: %s [-t timer_value]\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }
    if (apic_timer == 0 && !run_testfunc)
    {
        printf("%sNo timer value provided.\n", format_prefix);
        printf("%sProvide with [-t timer_value] or set option -f to find value dynamically.\n", format_prefix);
        exit(EXIT_FAILURE);
    }

    single_step_measure_args args = {
        .apic_timer = apic_timer,
        .format_prefix = format_prefix,
        .debug_enabled = debug_enabled};

    printf("Starting single step measure\n");
    if (run_testfunc)
    {
        testfunc(&args);
    }
    else
    {
        single_step_measure(&args);
    }
    return 0;
}