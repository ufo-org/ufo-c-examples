#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <assert.h>
#include <bzlib.h>

#include "bzip.h"
#include "ufo_c/target/ufo_c.h"

#define DEBUG
#ifdef DEBUG 
#define LOG(...) fprintf(stderr, "DEBUG: " __VA_ARGS__)
#define LOG_SHORT(...) fprintf(stderr,  __VA_ARGS__)
#else
#define LOG(...) 
#define LOG_SHORT(...) 
#endif
#define WARN(...) fprintf(stderr, "WARNING: " __VA_ARGS__)
#define REPORT(...) fprintf(stderr, "ERROR: " __VA_ARGS__)

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

// int main(int argc, char *argv[]) {
//     // Find all the blocks in the input file
//     Blocks *blocks = Blocks_parse("test/test2.txt.bz2");

//     for (size_t i = 0; i < blocks->blocks; i++) {
//         // Extract a single compressed block
//         Block *block = Block_from(blocks, i);

//         // Create the structures for outputting the decompressed data into
//         size_t output_buffer_size = 1024 * 1024 * 1024; // 1MB
//         char *output_buffer = (char *) calloc(output_buffer_size, sizeof(char));

//         int output_buffer_occupancy = Block_decompress(block, output_buffer_size, output_buffer);
//         printf("%d: %s", output_buffer_occupancy, output_buffer);
//     }
// }