#include <time.h>
#include "timing.h"

long current_time_in_ms() {
    struct timespec measurement;
    clock_gettime(CLOCK_MONOTONIC_RAW, &measurement);
    return (long) measurement.tv_sec * 1000 + (long) measurement.tv_nsec / 1000000;
}