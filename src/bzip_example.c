#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <assert.h>
#include <bzlib.h>

#include "logging.h"
#include "bzip.h"
#include "ufo_c/target/ufo_c.h"

#define HIGH_WATER_MARK (2L * 1024 * 1024 * 1024)
#define LOW_WATER_MARK  (1L * 1024 * 1024 * 1024)
#define MIN_LOAD_COUNT  (900L * 1024) // around block size, so around 900kB

int main(int argc, char *argv[]) {
    // Create UFO system
    UfoCore ufo_system = ufo_new_core("/tmp/", HIGH_WATER_MARK, LOW_WATER_MARK);
    if (ufo_core_is_error(&ufo_system)) {
        exit(1);
    }

    // Create UFO object
    BZip2 *object = BZip2_ufo_new(&ufo_system, "test/test2.txt.bz2", true, MIN_LOAD_COUNT);
    if (object == NULL) {
        exit(2);
    }
    
    // Iterate over everything.
    for (size_t i = 0; i < object->size; i++) {
        printf("%c", object->data[i]);
    }
    printf("\n");

    // Cleanup
    BZip2_ufo_free(&ufo_system, object);
    ufo_core_shutdown(ufo_system);
}