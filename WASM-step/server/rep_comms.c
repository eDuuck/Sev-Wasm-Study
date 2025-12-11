#include <stdio.h>
#include <stdint.h>
#include "attack_breakpoints.h"
#include <time.h>
volatile uint64_t memory_value = 0xCAFEBABE;

void perform_operations() {
    //start_measurement_breakpoint(); // Start measurement breakpoint
        __asm__ volatile (
        "mov $0, %%rax\n\t"
        "mov $0, %%rbx\n\t"
        "mov $0, %%rcx\n\t"
        "mov $0, %%rdx\n\t"
        "mov $0, %%rsi\n\t"
        "mov $0, %%rdi\n\t"
        "mov $0, %%r11\n\t"       // Instruction counter for reads
        "lea memory_value(%%rip), %%r8\n\t"   // Load address of memory_value into r8
"1:\n\t"
        "rdrand %%rax\n\t"
        "mov %%rax, (%%r8)\n\t"
        "add %%rax, %%rax\n\t"
        "mul %%rax\n\t"
        "add $1, %%r11\n\t"       // Increment instruction counter
        "cmp $0xF4240, %%r11\n\t"
        "jne 1b\n\t"
        
        :
        :
        : "rax", "rbx", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10", "r11", "memory"
    );
    stop_measurement_breakpoint(); // Stop measurement breakpoint
}

int main() {
    //printf("Starting...\n");
    start_measurement_breakpoint();
    clock_t start = clock();
    perform_operations();
    clock_t end = clock();
    float seconds = (float)(end - start) / CLOCKS_PER_SEC;
    printf("Execution time %ld us.\n", end - start);
    //printf("Done!\n");
    return 0;
}