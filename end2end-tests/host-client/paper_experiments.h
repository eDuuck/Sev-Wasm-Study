#ifndef PAPER_EXPERIMENTS_H
#define PAPER_EXPERIMENTS_H

#include <stdio.h>
#include <stdlib.h>

#include "../../sev-step-lib/ansi-color-codes.h"
#include "../../sev-step-lib/sev_step_api.h"

#include "my-error-codes.h"
#include "vm-server-client.h"
#include "helpers.h"
#include "test_definitions.h"

typedef struct
{
    int timer_value;
    // if true, we expect step events to contain rip data and check them against the expected offsets
    bool check_debug_rip;
    // whether guest tlb should be flushed before each step
    bool flush_tlb;
    // whether accessed bit of page containing the nop slide should be reset
    bool reset_access_bit;
} paper_experiments_nop_slide_args_t;
int paper_experiments_nop_slide(char *format_prefix, void *void_args)
{
    //
    // Initialize API connection and prepare tracked pages on server/vm side
    //
    paper_experiments_nop_slide_args_t *args = (paper_experiments_nop_slide_args_t *)void_args;

    usp_poll_api_ctx_t ctx;
    int res = HOST_CLIENT_SUCCESS;
    usp_event_type_t event_type;
    void *event_buffer = NULL;

    bool api_open = false;
    printf("%sInitializing API...\n", format_prefix);
    if (SEV_STEP_ERR == usp_new_ctx(&ctx, args->check_debug_rip))
    {
        printf("%s" BRED "usp_new_ctx failed." reset " Check dmesg for more information\n", format_prefix);
        return HOST_CLIENT_ERROR;
    }
    printf("%sAPI initialization done!\n", format_prefix);
    api_open = true;

    victim_program_data_t *victim_program_data = malloc(sizeof(victim_program_data_t));
    printf("%scalling vm_server_single_step_victim_init to prepare scenario\n", format_prefix);
    if (HOST_CLIENT_ERROR == vm_server_single_step_victim_init(VICTIM_PROGRAM_NOP_SLIDE, victim_program_data))
    {
        printf("%svm_server_single_step_victim_init " BRED "FAILED\n" reset, format_prefix);
        res = HOST_CLIENT_ERROR;
        goto cleanup;
    }
    // TODO: +4096 is not guaranteed to work here!
    uint64_t victim_pages[] = {victim_program_data->gpa};
    int victim_pages_len = sizeof(victim_pages) / sizeof(victim_pages[0]);
    printf("%s Victim program: VADDR is 0x%lx\n",
           format_prefix, victim_program_data->vaddr);

    //
    // Exec track gpa of victim function and signal vm server to start victim program async
    // Then, enter main event loop
    //

    printf("%stracking victim pages with mode %s\n", format_prefix, tracking_mode_to_string(KVM_PAGE_TRACK_EXEC));
    if (SEV_STEP_OK != track_pages(&ctx, victim_pages, victim_pages_len, KVM_PAGE_TRACK_EXEC))
    {
        printf("%sinitial track pages FAILED\n" reset, format_prefix);
        res = HOST_CLIENT_ERROR;
        goto cleanup;
    }

    printf("%scalling vm_server_single_step_victim_start\n", format_prefix);
    if (HOST_CLIENT_ERROR == vm_server_single_step_victim_start(VICTIM_PROGRAM_NOP_SLIDE))
    {
        printf("%svm_server_single_step_victim_start " BRED "FAILED\n" reset, format_prefix);
        res = HOST_CLIENT_ERROR;
        goto cleanup;
    }

    printf("%sentering main event loop\n", format_prefix);

    // if true, the last page fault event belonged to the target pages
    bool on_victim_pages = false;

    uint64_t zero_steps_on_target = 0;
    uint64_t single_steps_on_target = 0;
    uint64_t multi_steps_on_target = 0;
#define MULT_STEP_SIZES_MAX_LEN 1000
    uint64_t multi_step_sizes[MULT_STEP_SIZES_MAX_LEN];
    uint64_t multi_step_sizes_len = 0;

    /*We expect the victim program do be done after this many events. Contains some slack to account for scheduling and zero steps
    vicim_program_data.expected_offsets_len is the exact number of instructions/single step events for the target
    */
    int upper_event_thresh = 2 * victim_program_data->expected_offsets_len;
    // if true, we have single stepped through the whole target program successfully
    bool finished_single_stepping_target = false;

    for (int current_event_idx = 0; current_event_idx < upper_event_thresh && !finished_single_stepping_target; current_event_idx++)
    {

        printf("%sWaiting next event, event_idx=%d\n", format_prefix, current_event_idx);
        if (SEV_STEP_ERR == usp_block_until_event(&ctx, &event_type, &event_buffer))
        {
            printf("%susp_block_until_event" BRED " FAILED\n" reset, format_prefix);
            res = HOST_CLIENT_ERROR;
            goto cleanup;
        }

        if (event_type == PAGE_FAULT_EVENT)
        {
            usp_page_fault_event_t *pf_event = (usp_page_fault_event_t *)event_buffer;
            printf("%s Pagefault Event: {GPA:0x%lx}\n", format_prefix, pf_event->faulted_gpa);

            if (on_victim_pages)
            {
                if (elem_is_in_array(pf_event->faulted_gpa, victim_pages, victim_pages_len))
                {
                    printf("%s on_victim_pages=true but got fault for victim page! This should not happen\n", format_prefix);
                    res = HOST_CLIENT_ERROR;
                    goto cleanup;
                }
                else
                {
                    printf("%s Left victim pages with fault at GPA 0x%lx. Disabling single stepping and re-tracking victim pages\n",
                           format_prefix, pf_event->faulted_gpa);
                    if (SEV_STEP_OK != disable_single_stepping(&ctx))
                    {
                        printf("%sfailed to disable single stepping\n", format_prefix);
                        res = HOST_CLIENT_ERROR;
                        goto cleanup;
                    }

                    // track only the target page
                    if (SEV_STEP_OK != untrack_all_pages(&ctx, KVM_PAGE_TRACK_EXEC))
                    {
                        printf("%suntrack_all_pages failed\n", format_prefix);
                        res = HOST_CLIENT_ERROR;
                        goto cleanup;
                    }
                    if (SEV_STEP_OK != track_pages(&ctx, victim_pages, victim_pages_len, KVM_PAGE_TRACK_EXEC))
                    {
                        printf("%strack pages FAILED\n" reset, format_prefix);
                        res = HOST_CLIENT_ERROR;
                        goto cleanup;
                    }
                    on_victim_pages = false;

                    print_step_histogram_args_t histogram_args = {
                        .want_runs = 1,
                        .timer_value = args->timer_value,
                        .total_zero_step_count = zero_steps_on_target,
                        .total_single_step_count = single_steps_on_target,
                        .total_multi_step_count = multi_steps_on_target,
                        .total_step_count = zero_steps_on_target + single_steps_on_target + multi_steps_on_target,
                    };
                    print_step_histogram(format_prefix, histogram_args);
                    if (multi_step_sizes_len > 0)
                    {
                        printf("%smulti_step_sizes = [", format_prefix);
                        for (uint64_t i = 0; i < multi_step_sizes_len; i++)
                        {
                            printf("%ju, ", multi_step_sizes[i]);
                        }
                        printf("]\n");
                    }
                    printf("%s access bit reset trick:%d flush tlb trick: %d\n", format_prefix, args->reset_access_bit, args->flush_tlb);
                }
            }
            else
            { // currently not on victim pages
                if (elem_is_in_array(pf_event->faulted_gpa, victim_pages, victim_pages_len))
                {
                    printf("%s entering victim pages. enabling single stepping (timer 0x%x) and tracking all but the target page\n",
                           format_prefix, args->timer_value);

                    // track all pages but the target page
                    if (SEV_STEP_OK != track_all_pages(&ctx, KVM_PAGE_TRACK_EXEC))
                    {
                        printf("%strack_all_pages failed\n", format_prefix);
                        res = HOST_CLIENT_ERROR;
                        goto cleanup;
                    }
                    if (SEV_STEP_OK != untrack_pages(&ctx, victim_pages, victim_pages_len, KVM_PAGE_TRACK_EXEC))
                    {
                        printf("%suntrack_pages failed\n", format_prefix);
                        res = HOST_CLIENT_ERROR;
                        goto cleanup;
                    }

                    uint64_t *reset_access_bit_gpas = NULL;
                    uint64_t reset_access_bit_gpas_len = 0;
                    if (args->reset_access_bit)
                    {
                        reset_access_bit_gpas = victim_pages;
                        reset_access_bit_gpas_len = victim_pages_len;
                    }
                    if (SEV_STEP_OK != enable_single_stepping_ex(&ctx, args->timer_value, reset_access_bit_gpas, reset_access_bit_gpas_len, args->flush_tlb))
                    {
                        printf("%sfailed to enable single stepping\n", format_prefix);
                        res = HOST_CLIENT_ERROR;
                        goto cleanup;
                    }

                    on_victim_pages = true;
                }
                else
                {
                    printf("currently not on victim pages and page fault is not on victim pages : nothing to do\n");
                }
            }
        }
        else if (event_type == SEV_STEP_EVENT)
        {
            sev_step_event_t *step_event = (sev_step_event_t *)event_buffer;
            print_single_step_event(format_prefix, step_event);

            if (!on_victim_pages)
            {
                printf("%s on_victim_pages=false but single step event. This should not happen\n", format_prefix);
                res = HOST_CLIENT_ERROR;
                goto cleanup;
            }

            switch (step_event->counted_instructions)
            {
            case 0:
                zero_steps_on_target += 1;
                break;
            case 1:
                single_steps_on_target += 1;

                if (args->check_debug_rip && (single_steps_on_target < victim_program_data->expected_offsets_len))
                {
                    if (!step_event->is_decrypted_vmsa_data_valid)
                    {
                        printf("%s" BHRED "NO VMSA DATA" reset " args->check_debug_rip was specified but VMSA data not valid\n", format_prefix);
                        res = HOST_CLIENT_ERROR;
                        goto cleanup;
                    }
                    uint64_t expected_vaddr = victim_program_data->vaddr + victim_program_data->expected_offsets[single_steps_on_target - 1];
                    uint64_t rip = step_event->decrypted_vmsa_data.register_values[VRN_RIP];
                    if ((rip >> 12) != (expected_vaddr >> 12))
                    {
                        printf("%s" BHRED "vaddr check failed: page part of vaddr is wrong, single_steps_on_target = %ju, expected vaddr 0x%lx got 0x%lx\n" reset,
                               format_prefix, single_steps_on_target, expected_vaddr, rip);
                        // res = HOST_CLIENT_ERROR;
                        // goto cleanup;
                    }
                    if (rip != expected_vaddr)
                    {
                        printf("%s" BHRED "vaddr check failed:  single_steps_on_target = %ju,"
                               " page part correct, expected page offset 0x%lx got 0x%lx\n" reset,
                               format_prefix, single_steps_on_target, expected_vaddr & 0xfff, rip & 0xfff);
                        // res = HOST_CLIENT_ERROR;
                        // goto cleanup;
                    }
                    printf("%s step event is at expected vaddr\n", format_prefix);
                }

                if (single_steps_on_target > victim_program_data->expected_offsets_len)
                {
                    printf("%sFinished single stepping target program !\n", format_prefix);
                    finished_single_stepping_target = true;
                }

                break;
            default:
                printf("%s" BHRED "MULTISTEP" reset, format_prefix);
                multi_steps_on_target += 1;
                if (multi_step_sizes_len < MULT_STEP_SIZES_MAX_LEN)
                {
                    multi_step_sizes[multi_step_sizes_len] = step_event->counted_instructions;
                    multi_step_sizes_len += 1;
                }
                else
                {
                    flf_printf("multi_step_sizes overflow! Not storing step size\n");
                }

                // res = HOST_CLIENT_ERROR;
                // goto cleanup;
                break;
            }
        }
        else
        {
            printf("%s Unexpected event type! Aborting\n", format_prefix);
            res = HOST_CLIENT_ERROR;
            goto cleanup;
        }

        printf("%ssending ack for event_idx %d\n", format_prefix, current_event_idx);
        usp_ack_event(&ctx);
        free_usp_event(event_type, event_buffer);
        event_buffer = NULL;

    } // end of main event loop

    printf("%sClosing API...\n", format_prefix);
    if (SEV_STEP_ERR == usp_close_ctx(&ctx))
    {
        printf("%s" BRED "usp_close_ctx failed." reset " Check dmesg for more information\n", format_prefix);
        res = HOST_CLIENT_ERROR;
        api_open = false;
        goto cleanup;
    }
    else
    {
        printf("%sAPI closed!\n", format_prefix);
    }
    api_open = false;

    print_step_histogram_args_t histogram_args = {
        .want_runs = 1,
        .timer_value = args->timer_value,
        .total_zero_step_count = zero_steps_on_target,
        .total_single_step_count = single_steps_on_target,
        .total_multi_step_count = multi_steps_on_target,
        .total_step_count = zero_steps_on_target + single_steps_on_target + multi_steps_on_target,
    };
    print_step_histogram(format_prefix, histogram_args);
    if (multi_step_sizes_len > 0)
    {
        printf("%smulti_step_sizes = [", format_prefix);
        for (uint64_t i = 0; i < multi_step_sizes_len; i++)
        {
            printf("%ju, ", multi_step_sizes[i]);
        }
        printf("]\n");
    }
    printf("%s access bit reset trick:%d flush tlb trick: %d\n", format_prefix, args->reset_access_bit, args->flush_tlb);


    if (!finished_single_stepping_target)
    {
        printf("%s" BHRED "Did not finish target!. Wanted %lu single step events on target, got only %ju\n" reset,
               format_prefix, victim_program_data->expected_offsets_len, single_steps_on_target);
        res = HOST_CLIENT_ERROR;
        goto cleanup;
    };

    //
    // interact with the vm to ensure that it is not stuck
    //
    bool vm_alive = false;
    for (int retries = 1; !vm_alive && retries >= 0; retries--)
    {
        victim_program_data_t *_dummy = malloc(sizeof(victim_program_data_t));
        printf("%ssending dummy request to vm to ensure that it is alive and well\n", format_prefix);
        if (HOST_CLIENT_ERROR == vm_server_single_step_victim_init(VICTIM_PROGRAM_NOP_SLIDE, _dummy))
        {
            printf("%sdummy request " BRED "FAILED. %d retries remaining\n" reset, format_prefix, retries);
        }
        else
        {
            vm_alive = true;
        }
        free_victim_program_data_t(_dummy);
    }
    if (vm_alive)
    {
        printf("%s VM seems to be alive and well\n", format_prefix);
    }
    else
    {
        printf("%svm is stuck\n", format_prefix);
        res = HOST_CLIENT_ERROR;
        goto cleanup;
    }

cleanup:
    if (api_open)
    {
        printf("%sClosing API...\n", format_prefix);
        if (SEV_STEP_ERR == usp_close_ctx(&ctx))
        {
            printf("%s" BRED "usp_close_ctx failed." reset " Check dmesg for more information\n", format_prefix);
            res = HOST_CLIENT_ERROR;
        }
        else
        {
            printf("%sAPI closed!\n", format_prefix);
        }
    }

    if (event_buffer != NULL)
    {
        free_usp_event(event_type, event_buffer);
    }
    free_victim_program_data_t(victim_program_data);

    return res;
}

#endif