#include <stdint.h>

// Small recursive function
int fib_mod(int n, int mod) {
    if (n <= 1) return n;
    return (fib_mod(n - 1, mod) + fib_mod(n - 2, mod)) % mod;
}

// Tiny hash-like mixer
uint32_t mix(uint32_t x) {
    x ^= x >> 13;
    x *= 0x5bd1e995;
    x ^= x >> 15;
    return x;
}

// Simple prime-ish check
int is_primeish(int n) {
    if (n < 2) return 0;
    for (int i = 2; i * i <= n; i++) {
        if (n % i == 0) return 0;
    }
    return 1;
}

int func() {
    int res = 0;

    // 1. Math and bit ops
    int a = 23, b = 47;
    res += (a * b) ^ (a + b);

    // 2. Array sum and XOR pattern
    int arr[6] = {3, 1, 4, 1, 5, 9};
    int s = 0;
    for (int i = 0; i < 6; i++) {
        s ^= (arr[i] + i * i);
    }
    res ^= s;

    // 3. Small Fibonacci recursion
    res += fib_mod(10, 97);

    // 4. Bit manipulation fun
    uint32_t x = 0xDEADBEEF;
    x = mix(x);
    res ^= (x & 0xFFFF);

    // 5. Conditional shuffle
    if ((x >> 8) & 1)
        res += (a << 2);
    else
        res -= (b >> 1);

    // 6. Mini checksum loop
    uint8_t bytes[8] = {0xA, 0xB, 0xC, 0xD, 0xE, 0xF, 0x1, 0x2};
    uint32_t checksum = 0;
    for (int i = 0; i < 8; i++) {
        checksum = (checksum << 1) ^ bytes[i];
        checksum ^= (checksum >> 3);
    }
    res ^= checksum;

    // 7. Pseudo prime counter
    int prime_count = 0;
    for (int i = 10; i < 30; i++) {
        prime_count += is_primeish(i);
    }
    res += prime_count * 13;

    // 8. Tiny pseudo-random feedback
    for (int i = 0; i < 5; i++) {
        res = (res * 37 + 11) ^ (res >> 3);
    }

    // 9. Final normalization
    res &= 0x7FFFFFFF; // keep it positive
    return res % 13371337;
}
