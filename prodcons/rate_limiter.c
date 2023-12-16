#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>

#define usec_t unsigned long long

typedef struct {
    // TODO: thread-safe
    pthread_mutex_t mutex;
    double interval;
    usec_t next_free;

} RateLimiter;

int min(int a, int b) {
    return a <= b ? a : b;
}

usec_t now() {
    struct timeval now_tv;
    gettimeofday(&now_tv, NULL);
    return now_tv.tv_sec * 1000000 + now_tv.tv_usec; 
}

usec_t claim_next(RateLimiter *rate_limiter, int permits) {
    usec_t now_ts = now();

    // No burst
    if (now_ts > rate_limiter->next_free) {
        rate_limiter->next_free = now_ts;
    }

    usec_t wait_time = rate_limiter->next_free - now_ts;
 
    rate_limiter->next_free += permits * (usec_t) rate_limiter->interval;

    return wait_time;
}

usec_t acquire_permits(RateLimiter *rate_limiter, int permits) {
    usec_t wait_time = claim_next(rate_limiter, permits);
    usleep(wait_time);
    return wait_time;
}

usec_t acquire(RateLimiter *rate_limiter) {
    return acquire_permits(rate_limiter, 1);
}

void set_rate(RateLimiter *rate_limiter, double rate) {
    rate_limiter->interval = 1000000.0 / rate;
}

double get_rate(RateLimiter *rate_limiter) {
    return 1000000.0 / rate_limiter->interval;
}

RateLimiter *get_rate_limiter(double rate) {
    RateLimiter *rate_limiter = (RateLimiter *) malloc(sizeof(RateLimiter));

    pthread_mutex_init(&rate_limiter->mutex, NULL);
    set_rate(rate_limiter, rate);
    rate_limiter->next_free = now();

    return rate_limiter;
}
