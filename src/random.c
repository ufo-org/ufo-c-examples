#include <stdlib.h>

int random_int(int ceiling) {
    return rand() % ceiling;
}

size_t random_index(size_t ceiling) {
    size_t high = (size_t) rand();
    size_t low  = (size_t) rand();
    size_t big = (high << 32) | low;
    return big % ceiling;
}