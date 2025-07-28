#include <stdio.h>
#include <stdint.h>
#include "attack_breakpoints.h"

volatile uint64_t memory_value = 0xCAFEBABE;

void perform_operations() {
    volatile int a = 0;
    start_measurement_breakpoint(); // Start measurement breakpoint
    for(int i = 0; i < 10000; i++) {
        a += i; // Some operation to ensure the loop is not optimized out.
    }
    stop_measurement_breakpoint(); // Stop measurement breakpoint

}

int main() {
    //printf("Starting...\n");
    perform_operations();
    //printf("Done!\n");
    return 0;
}