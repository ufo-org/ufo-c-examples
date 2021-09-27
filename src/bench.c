#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <argp.h>

#include "ufo_c/target/ufo_c.h"
#include "timing.h"

#include "fib.h"

#define GB (1024L * 1024L * 1024L)
#define MB (1024L * 1024L)
#define KB (1024L)

#define HIGH_WATER_MARK (2L * 1024 * 1024 * 1024)
#define LOW_WATER_MARK  (1L * 1024 * 1024 * 1024)
#define MIN_LOAD_COUNT  (4L * 1024)

typedef struct {
    char *benchmark;
    char *implementation;
    char *pattern;
    char *file;
    char *timing;
    size_t size;
    size_t sample_size;
    size_t min_load;
    size_t high_water_mark;
    size_t low_water_mark;    
} Arguments;

static error_t parse_opt (int key, char *value, struct argp_state *state) {
    /* Get the input argument from argp_parse, which we
        know is a pointer to our arguments structure. */
    Arguments *arguments = state->input;
    switch (key) {
        case 'b': arguments->benchmark = value; break;
        case 'i': arguments->implementation = value; break;
        case 's': arguments->size = (size_t) atol(value); break;
        case 'm': arguments->min_load = (size_t) atol(value); break;
        case 'h': arguments->high_water_mark = (size_t) atol(value); break;
        case 'l': arguments->low_water_mark = (size_t) atol(value); break;
        case 'o': arguments->file = value; break;
        case 't': arguments->timing = value; break;
        case 'p': arguments->pattern = value; break;
        case 'n': arguments->sample_size = (size_t) atol(value); break;
        default:  return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

typedef void *AnySystem;
typedef void *AnyObject;

// SETUP
typedef void *(*system_setup_t)(Arguments *);
void *ufo_setup(Arguments *config) {
    UfoCore ufo_system = ufo_new_core("/tmp/", config->high_water_mark, config->low_water_mark);
    if (ufo_core_is_error(&ufo_system)) {
        printf("ERROR: Cannot create UFO core system.\n");
        exit(1);
    }
    UfoCore *ptr = (UfoCore *) malloc(sizeof(UfoCore));
    *ptr = ufo_system;
    return ptr;
}
void *normil_setup(Arguments *config) {
    return NULL;
}
void *ny_setup(Arguments *config) {
    printf("ny_setup unimplemented!\n");
    exit(99);
}

// TEARDOWN
typedef void (*system_teardown_t)(Arguments *, AnySystem);
void ufo_teardown(Arguments *config, AnySystem system) {
    UfoCore *ufo_system_ptr = (UfoCore *) system;
    ufo_core_shutdown(*ufo_system_ptr);
    free(ufo_system_ptr);
}
void normil_teardown(Arguments *config, AnySystem system) {
    /* empty */
}
void ny_teardown(Arguments *config, AnySystem system) {
    printf("ny_teardown unimplemented!\n");
    exit(99);
}

// OBJECT CREATION
typedef void *(*object_creation_t)(Arguments *, AnySystem);
void *normil_fib_creation(Arguments *config, AnySystem system) {
    return (void *) normil_fib_new(config->size);
}
void *ufo_fib_creation(Arguments *config, AnySystem system) {
    UfoCore *ufo_system_ptr = (UfoCore *) system;
    return (void *) ufo_fib_new(ufo_system_ptr, config->size);
}
void *ny_fib_creation(Arguments *config, AnySystem system) {
    printf("ny_fib_creation unimplemented!\n");
    exit(99);
}

// OBJECT CLEANUP
typedef void (*object_cleanup_t)(Arguments *, AnySystem, AnyObject);
void normil_fib_cleanup(Arguments *config, AnySystem system, AnyObject object) {
    normil_fib_free(object);
}
void ufo_fib_cleanup(Arguments *config, AnySystem system, AnyObject object) {
    UfoCore *ufo_system_ptr = (UfoCore *) system;
    ufo_fib_free(ufo_system_ptr, object);
}
void ny_fib_cleanup(Arguments *config, AnySystem system, AnyObject object) {
    printf("ny_fib_cleanup unimplemented!\n");
    exit(99);
}

// EXECUTION
// TODO

int main(int argc, char *argv[]) {

    Arguments config;
    /* Default values. */
    config.benchmark = "seq";
    config.implementation = "normil";
    config.size = 1 *KB;
    config.min_load = 4 *KB;
    config.high_water_mark = 2 *GB;
    config.low_water_mark = 1 *GB;
    config.file = "test/test.txt.bz2";
    config.timing = "timing.csv";
    config.pattern = "scan";
    config.sample_size = 0; // 0 for all

    // Parse arguments
    static char doc[] = "UFO performance benchmark utility.";
    static char args_doc[] = "";
    static struct argp_option options[] = {
        {"benchmark",       'b', "BENCHMARK",      0,  "Benchmark (populate function) to run: seq, fib, postgres, or bzip"},
        {"implementation",  'i', "IMPL",           0,  "Implementation to run: ufo , ny, normil"},
        {"pattern",         'p', "FILE",           0,  "Read pattern: scan, random"},
        {"sample-size",     'n', "FILE",           0,  "How many elements to read from vector: zero for all"},
        {"size",            's', "#B",             0,  "Vector size (applicable for fib and seq)"},        
        {"file",            'f', "FILE",           0,  "Input file (applicable for bzip)"},
        {"min-load",        'm', "#B",             0,  "Min load count for ufo"},
        {"high-water-mark", 'h', "#B",             0,  "High water mark for ufo GC"},
        {"low-water-mark",  'l', "#B",             0,  "Low water mark for ufo GC"},
        {"timing",          't', "FILE",           0,  "Path of CSV output file for time measurements"},        
        { 0 }
    };
    static struct argp argp = { options, parse_opt, args_doc, doc };   
    argp_parse (&argp, argc, argv, 0, 0, &config);

    // Check.
    printf("Benchmark configuration:\n");
    printf("  * benchmark:       %s\n",  config.benchmark      );
    printf("  * implementation:  %s\n",  config.implementation );
    printf("  * size:            %lu\n", config.size           );
    printf("  * min_load:        %lu\n", config.min_load       );
    printf("  * high_water_mark: %lu\n", config.high_water_mark);
    printf("  * low_water_mark:  %lu\n", config.low_water_mark );
    printf("  * file:            %s\n",  config.file           );
    printf("  * timing:          %s\n",  config.timing         );

    // Setup and teardown;
    system_setup_t system_setup = NULL;
    system_teardown_t system_teardown = NULL;
    if (strcmp(config.implementation, "ufo") == 0) {
        system_setup = &ufo_setup;
        system_teardown = &ufo_teardown;
    }
    if (strcmp(config.implementation, "ny") == 0) {
        system_setup = &ny_setup;
        system_teardown = &ny_teardown;
    }
    if (strcmp(config.implementation, "normil") == 0) {
        system_setup = &normil_setup;
        system_teardown = &normil_teardown;
    }
    if (system_setup == NULL || system_teardown == NULL) {
        printf("Unknown implementation \"%s\"\n", config.implementation);
        return 4;
    }

    // Object creation and teardown.
    object_creation_t object_creation = NULL;
    object_cleanup_t object_cleanup = NULL;
    if ((strcmp(config.benchmark, "fib") == 0) && (strcmp(config.implementation, "ufo") == 0)) {
        object_creation = ufo_fib_creation;
        object_cleanup = ufo_fib_cleanup;
    }
    if ((strcmp(config.benchmark, "fib") == 0) && (strcmp(config.implementation, "ny") == 0)) {
        object_creation = ny_fib_creation;
        object_cleanup = ny_fib_cleanup;
    }
    if ((strcmp(config.benchmark, "fib") == 0) && (strcmp(config.implementation, "normil") == 0)) {
        object_creation = normil_fib_creation;
        object_cleanup = normil_fib_cleanup;
    }
    if (object_creation == NULL || object_cleanup == NULL) {
        printf("Unknown benchmark/implementation combination \"%s\"/\"%s\"\n", 
        config.benchmark, config.implementation);
        return 4;
    }

    // System setup
    printf("System setup\n");
    long system_setup_start_time = current_time_in_ms();
    void* system = system_setup(&config);
    long system_setup_elapsed_time = current_time_in_ms() - system_setup_start_time;

    // Object creation
    printf("Object creation\n");
    long object_creation_start_time = current_time_in_ms();
    long object_creation_elapsed_time = current_time_in_ms() - object_creation_start_time;

    // Execution
    printf("Execution\n");
    long execution_start_time = current_time_in_ms();
    long execution_elapsed_time = current_time_in_ms() - execution_start_time;

    // Object cleanup
    printf("Object cleanup\n");
    long object_cleanup_start_time = current_time_in_ms();
    long object_cleanup_elapsed_time = current_time_in_ms() - object_cleanup_start_time;
    
    // System teardown
    printf("System teardown\n");
    long system_teardown_start_time = current_time_in_ms();
    system_teardown(&config, system);
    long system_teardown_elapsed_time = current_time_in_ms() - system_teardown_start_time;

    // Output (TODO: to CSV)
    printf("benchmark,"
           "implementation,"
           "system_setup_time,"
           "object_creation_time,"
           "execution_time,"
           "object_cleanup_time,"
           "system_teardown_time\n");

    printf("%s,%s,%lu,%lu,%lu,%lu,%lu\n",
        config.benchmark,
        config.implementation,
        system_setup_elapsed_time,
        object_creation_elapsed_time,
        execution_elapsed_time,
        object_cleanup_elapsed_time,
        system_teardown_elapsed_time);
        
    // if (strcmp(config.benchmark, "seq") == 0) {
        
    // }
    // if (system_setup == NULL) {
    //     printf("ERROR: unknown benchmark: %s\n", );
    //     exit(3);
    // }

    // if (strcmp(benchmark, "fib")) {
        
    // }

    // if (strcmp(benchmark, "postgres")) {
        
    // }

    // if (strcmp(benchmark, "bzip")) {
        
    // }

}

    // // Sytem setup
    // UfoCore ufo_system = ufo_new_core("/tmp/", HIGH_WATER_MARK, LOW_WATER_MARK);
    // if (ufo_core_is_error(&ufo_system)) {
    //     exit(1);
    // }

    // // Object creation
    // uint64_t *fib = CONSTRUCTOR(&ufo_system, config.size);
    // if (fib == NULL) {
    //     exit(1);
    // }

    // // Benchmark guts: some pattern of memory accesses
    // for (size_t i = 0; i < size; i++) {
    //     printf("%lu -> %lu\n", i, fib[i]);
    // }

    // // Object destruction
    // fib_free(&ufo_system, fib);

    // // System shutdown
    // ufo_core_shutdown(ufo_system);