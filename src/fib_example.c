#include "fib.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "ufo_c/target/ufo_c.h"

#define HIGH_WATER_MARK (2L * 1024 * 1024 * 1024)
#define LOW_WATER_MARK  (1L * 1024 * 1024 * 1024)
#define MIN_LOAD_COUNT  (4L * 1024)

int main(int argc, char *argv[]) {
    UfoCore ufo_system = ufo_new_core("/tmp/", HIGH_WATER_MARK, LOW_WATER_MARK);
    if (ufo_core_is_error(&ufo_system)) {
        exit(1);
    }

    size_t size = 100000;
    uint64_t *fib = ufo_fib_new(&ufo_system, size);
    if (fib == NULL) {
        exit(1);
    }
    for (size_t i = 0; i < size; i++) {
        printf("%lu -> %lu\n", i, fib[i]);
    }

    ufo_fib_free(&ufo_system, fib);
    ufo_core_shutdown(ufo_system);
}
