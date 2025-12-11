#include <stdio.h>
#include <stdint.h>
#include "attack_breakpoints.h"
#include <time.h>
#define LOOP_AMOUNT 1000000

uint64_t jump_table[256] = {0};
uint32_t memory_value[LOOP_AMOUNT] __attribute__((aligned(64)));
// volatile uint64_t memory_value = 0xCAFEBABE;

void perform_operations()
{
    // start_measurement_breakpoint(); // Start measurement breakpoint
start_measurement_breakpoint();
 __asm__ volatile (
    // Zero registers
    "xor %%rax, %%rax\n\t"
    "xor %%rbx, %%rbx\n\t"
    "xor %%rcx, %%rcx\n\t"
    "xor %%rdx, %%rdx\n\t"
    "xor %%rsi, %%rsi\n\t"
    "xor %%rdi, %%rdi\n\t"
    "xor %%r11, %%r11\n\t"
    "xor %%r12, %%r12\n\t"

    // Pointers
    "lea memory_value(%%rip), %%rbx\n\t"   // base ptr for loads/stores
    "lea jump_table(%%rip), %%r10\n\t"     // fake jump table
    "add $0xF4240*4, %%rbx\n\t"

"1:\n\t"
    // === Instruction TYPES matching your snippet ===
    "endbr64\n\t"

    "sub $0x4, %%rbx\n\t"                 // stack-ish decrement

    "mov (%%rbx), %%eax\n\t"              // memory read

    "add $0x1, %%rbp\n\t"                 // fake SP increment

    "add %%eax, -0x4(%%rbx)\n\t"          // write to memory

    "movzbl -0x1(%%rbx), %%eax\n\t"       // byte load + zero extend

    "mov (%%rbx), %%rax\n\t"      // indexed table load

    // Instead of actually jumping to RAX, just pretend and continue
    "/* fake jmp */\n\t"

    // Loop counter
    "add $1, %%r11\n\t"
    "cmp $0xF4240, %%r11\n\t"
    "jne 1b\n\t"

    :
    :
    : "rax","rbx","rcx","rdx","rsi","rdi","r8","r9","r10","r11","r12","memory"
);
    stop_measurement_breakpoint(); // Stop measurement breakpoint
}

int main()
{
    // printf("Starting...\n");
    //clock_t start = clock();
    perform_operations();
    //clock_t end = clock();
    //float seconds = (float)(end - start) / CLOCKS_PER_SEC;
    //printf("Execution time %ld us.\n", end - start);
    // printf("Done!\n");
    return 0;
}