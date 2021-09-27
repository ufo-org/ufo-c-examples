#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <argp.h>

#include "ufo_c/target/ufo_c.h"

#define GB (1024L * 1024L * 1024L)
#define MB (1024L * 1024L)
#define KB (1024L)

#define HIGH_WATER_MARK (2L * 1024 * 1024 * 1024)
#define LOW_WATER_MARK  (1L * 1024 * 1024 * 1024)
#define MIN_LOAD_COUNT  (4L * 1024)

uint64_t *normil_fib_new(size_t n) {
    uint64_t *target = (uint64_t *) malloc(sizeof(uint64_t) * n);

    target[0] = 1;
    target[1] = 1;

    for (size_t i = 2; i < n; i++) {
        target[i] = target[i - 1] + target[i - 2];
    }
    
    return target;
}

void normil_fib_free(uint64_t * ptr) {
    free(ptr);
}


struct arguments {
    char *benchmark;
    char *implementation;
    char *file;
    char *timing;
    size_t size;
    size_t min_load;
    size_t high_water_mark;
    size_t low_water_mark;    
};

static error_t parse_opt (int key, char *value, struct argp_state *state) {
    /* Get the input argument from argp_parse, which we
        know is a pointer to our arguments structure. */
    struct arguments *arguments = state->input;
    switch (key) {
        case 'b': arguments->benchmark = value; break;
        case 'i': arguments->implementation = value; break;
        case 's': arguments->size = (size_t) atol(value); break;
        case 'm': arguments->min_load = (size_t) atol(value); break;
        case 'h': arguments->high_water_mark = (size_t) atol(value); break;
        case 'l': arguments->low_water_mark = (size_t) atol(value); break;
        case 'o': arguments->file = value; break;
        case 't': arguments->timing = value; break;
        default:  return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

int main(int argc, char *argv[]) {

    struct arguments config;
    /* Default values. */
    config.benchmark = "seq";
    config.implementation = "normil";
    config.size = 1 *KB;
    config.min_load = 4 *KB;
    config.high_water_mark = 2 *GB;
    config.low_water_mark = 1 *GB;
    config.file = "test/test.txt.bz2";
    config.timing = "timing.csv";

    // Parse arguments
    static char doc[] = "UFO benchmark.";
    static char args_doc[] = "TODO";
    static struct argp_option options[] = {
        {"benchmark",       'b', "BENCHMARK",      0,  "Benchmark to run: seq, fib, postgres, or bzip"},
        {"implementation",  'i', "IMPLEMENTATION", 0,  "Implementation to run: ufo , ny, normil"},
        {"size",            's', "#BYTES",         0,  "Vector size (applicable for fib and seq)"},        
        {"file",            'f', "FILE",           0,  "Input file (applicable for bzip)"},
        {"min-load",        'm', "#BYTES",         0,  "Min load count for UFO"},
        {"high-water-mark", 'h', "#BYTES",         0,  "High water mark for UFO GC"},
        {"low-water-mark",  'l', "#BYTES",         0,  "Low water mark for UFO GC"},
        {"timing",          't', "FILE",           0,  "Path of CSV output file for time measurements"},
        { 0 }
    };
    static struct argp argp = { options, parse_opt, args_doc, doc };   
    argp_parse (&argp, argc, argv, 0, 0, &config);

    printf("Benchmark configuration:\n");
    printf("  * benchmark:       %s\n",  config.benchmark      );
    printf("  * implementation:  %s\n",  config.implementation );
    printf("  * size:            %lu\n", config.size           );
    printf("  * min_load:        %lu\n", config.min_load       );
    printf("  * high_water_mark: %lu\n", config.high_water_mark);
    printf("  * low_water_mark:  %lu\n", config.low_water_mark );
    printf("  * file:            %s\n",  config.file           );
    printf("  * timing:          %s\n",  config.timing         );

    // if (strcmp(benchmark, "seq")) {
        
    // }

    // if (strcmp(benchmark, "fib")) {
        
    // }

    // if (strcmp(benchmark, "postgres")) {
        
    // }

    // if (strcmp(benchmark, "bzip")) {
        
    // }

    // System setup

    // Object creation
    

    // Usage

    // Object destruction
}


    // UfoCore ufo_system = ufo_new_core("/tmp/", HIGH_WATER_MARK, LOW_WATER_MARK);
    // if (ufo_core_is_error(&ufo_system)) {
    //     exit(1);
    // }

    // size_t size = 100000;
    // uint64_t *fib = fib_new(&ufo_system, size);
    // if (fib == NULL) {
    //     exit(1);
    // }
    // for (size_t i = 0; i < size; i++) {
    //     printf("%lu -> %lu\n", i, fib[i]);
    // }

    // fib_free(&ufo_system, fib);
    // ufo_core_shutdown(ufo_system);