#include <stdio.h>
#include "rate_limiter.c"

#define second 1000000
#define TOLERANCE 10000 // 10 millisecond (1%)


usec_t get_error(usec_t expected, usec_t real) {
    if (expected > real) {
        return expected - real;
    }
    return real - expected;
}

// Return 0 (false) or 1 (true)
void assert_true(usec_t expected, usec_t real) {
    usec_t error = get_error(expected, real);
    int ans = error < TOLERANCE ? 1 : 0;
    if (ans) {
        printf("Passed\n");
    } else {
        printf("Failed\n");
    }
}

void test_acquire() {
    RateLimiter *rate_limiter = get_rate_limiter(1); // 1 permit per second

    usec_t start_ts = now();
    
    acquire(rate_limiter);  
    acquire(rate_limiter);  
    
    usec_t end_ts = now();
    usec_t elapsed_time = end_ts - start_ts;
    printf("elapsed time: %llu\n", elapsed_time / 1000);

    assert_true(1 * second, elapsed_time);
}

void test_acquire_permits() {
    RateLimiter *rate_limiter = get_rate_limiter(0.5); // 0.5 permit per second

    // Should wailt 4s after this
    acquire_permits(rate_limiter, 2);
    
    usec_t start_ts = now();

    acquire_permits(rate_limiter, 1);
    
    usec_t end_ts = now();
    usec_t elapsed_time = end_ts - start_ts;
    printf("elapsed time: %llu\n", elapsed_time / 1000);

    assert_true(4 * second, elapsed_time);
}

void test_rate_change() {
    RateLimiter *rate_limiter = get_rate_limiter(1); // 1 permit per second

    usec_t start_ts = now();

    acquire(rate_limiter);
    acquire(rate_limiter);

    usec_t end_ts = now();
    usec_t elapsed_time = end_ts - start_ts;
    printf("elapsed time: %llu\n", elapsed_time / 1000);

    assert_true(1 * second, elapsed_time);  

    // Change rate
    set_rate(rate_limiter, 0.5);

    start_ts = now();

    acquire(rate_limiter); // just wait for 1s because next_free does't change 
    acquire(rate_limiter); // after 2s
    acquire(rate_limiter); // after 2s

    end_ts = now();
    elapsed_time = end_ts - start_ts;
    printf("elapsed time: %llu\n", elapsed_time / 1000);

    assert_true(5 * second, elapsed_time);  
}

// Testing 
int main() {
    test_acquire();
    test_acquire_permits();
    test_rate_change();
    // TODO: test thread-safe 
}