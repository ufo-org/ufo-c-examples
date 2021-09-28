#include <time.h>
#include <stdint.h>
#include "timing.h"

uint64_t current_time_in_ns() {
    struct timespec measurement;
    clock_gettime(CLOCK_MONOTONIC_RAW, &measurement);
    return ((uint64_t) measurement.tv_sec) * 1000000000 + (uint64_t) measurement.tv_nsec;
}