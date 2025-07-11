#include <stdio.h>
#include <stdint.h>
#include "attack_breakpoints.h"

volatile uint64_t memory_value = 0xCAFEBABE;

void perform_operations() {
    start_measurement_breakpoint(); // Start measurement breakpoint
        __asm__ volatile (
        "mov $0, %%rax\n\t"
        "mov $0, %%rbx\n\t"
        "mov $0, %%rcx\n\t"
        "mov $0, %%rdx\n\t"
        "mov $0, %%rsi\n\t"
        "mov $0, %%rdi\n\t"
        "lea memory_value(%%rip), %%r8\n\t"   // Load address of memory_value into r8

        "mov $0, %%r11\n\t"       // Instruction counter for reads
"1:\n\t"
        "add $1, %%rax\n\t"
        "imul %%rax, %%rbx\n\t"
        "xor %%rbx, %%rcx\n\t"
        "or  %%rcx, %%rdx\n\t"
        "shl $1, %%rdx\n\t"
        "mov (%%r8), %%r9\n\t"     // Read from memory every 10 ops
        "mov $0, %%r9\n\t"        // Reset r9
        "add $1, %%r11\n\t"       // Increment instruction counter
        "cmp $200, %%r11\n\t"
        "jne 1b\n\t"
        
        :
        :
        : "rax", "rbx", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10", "r11", "memory"
    );
    stop_measurement_breakpoint(); // Stop measurement breakpoint
}

int main() {
    //printf("Starting...\n");
    perform_operations();
    //printf("Done!\n");
    return 0;
}