#include <stdint.h>

// Return a boolean after a small check
int is_even(int x) {
    return (x % 2) == 0;
}

// Return an integer after a few quick ops
int square_diff(int a, int b) {
    int d = a - b;
    return d * d + (a & b);
}

// Return a float approximation
float blend_ratio(int a, int b) {
    float fa = (float)a, fb = (float)b;
    return (fa / (fb + 1.0f)) * 0.75f;
}

// Return a double that depends on a nested call
double fancy_mix(double x) {
    double y = x * x - (x / 3.0);
    return y + blend_ratio((int)x, (int)(y));
}

// Modify by reference (void return)
void tweak_values(int *a, int *b) {
    *a = (*a * 3) ^ (*b + 5);
    *b = (*b << 1) + (*a >> 2);
}

// Return a uint32_t with bit play
uint32_t rotl(uint32_t x, int r) {
    return (x << r) | (x >> (32 - r));
}

// Nested boolean + int logic
int combined_logic(int x, int y) {
    if (is_even(x)) return square_diff(x, y);
    else return square_diff(y, x) / 2;
}

// Return a pseudo-randomish value
uint32_t scramble(uint32_t v) {
    v ^= v >> 13;
    v ^= v << 17;
    v ^= v >> 5;
    return v;
}

int func(int inp) {
    int a = inp, b = 42;
    tweak_values(&a, &b);

    int even = is_even(a + b);
    int diff = combined_logic(a, b);
    float ratio = blend_ratio(a, b);
    double mixed = fancy_mix(ratio + diff * 0.01);

    uint32_t s = scramble(rotl((uint32_t)(mixed * 1000), 5));
    int result = (int)(s ^ (uint32_t)(ratio * 100));

    if (even)
        result ^= (int)(mixed);
    else
        result += diff;

    result &= 0x7FFFFFFF;
    return result % 9999991;
}
