#include "seq.h"
#include "stdio.h"

#define HIGH_WATER_MARK (2L * 1024 * 1024 * 1024)
#define LOW_WATER_MARK  (1L * 1024 * 1024 * 1024)
#define MIN_LOAD_CT  (4L * 1024)

int main(int argc, char *argv[]) {
    UfoCore ufo_system = ufo_new_core("/tmp/", HIGH_WATER_MARK, LOW_WATER_MARK);
    size_t size = 10000;

    int64_t *seq = seq_ufo_from_length(&ufo_system, 1, size, 2, false, MIN_LOAD_CT);
    if (seq == NULL) {
        exit(1);
    }
    for (size_t i = 0; i < size; i++) {
        printf("%lu -> %li\n", i, seq[i]);
    }

    seq_ufo_free(&ufo_system, seq);
    ufo_core_shutdown(ufo_system);
}