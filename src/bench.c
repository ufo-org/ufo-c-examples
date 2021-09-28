#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <argp.h>
#include <sys/stat.h>
#include <locale.h>

#include "ufo_c/target/ufo_c.h"
#include "timing.h"

#include "fib.h"

#define GB (1024L * 1024L * 1024L)
#define MB (1024L * 1024L)
#define KB (1024L)

#define HIGH_WATER_MARK (2L * 1024 * 1024 * 1024)
#define LOW_WATER_MARK  (1L * 1024 * 1024 * 1024)
#define MIN_LOAD_COUNT  (4L * 1024)

bool file_exists (char *filename) {
  struct stat   buffer;   
  return (stat (filename, &buffer) == 0);
}

int random_int(int ceiling) {
    return rand() % ceiling;
}

size_t random_index(size_t ceiling) {
    size_t high = (size_t) rand();
    size_t low  = (size_t) rand();
    size_t big = (high << 32) | low;
    return big % ceiling;
}

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
    size_t writes;
    unsigned int seed;
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
        case 'w': arguments->writes = (size_t) atol(value); break;
        case 'S': arguments->seed = (unsigned int) atoi(value); break;
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

// Sequence iterators
typedef void *AnySequence;
typedef struct { size_t current; bool end; bool write; } SequenceResult;
typedef SequenceResult (*sequence_t)(Arguments *, AnySequence);

typedef struct { 
    size_t current; 
    size_t length; 
    size_t since_last_write; 
} ScanSequence;
SequenceResult ScanSequence_next(Arguments *config, AnySequence sequence) {
    ScanSequence *scan_sequence = (ScanSequence *) sequence;
    SequenceResult result;
    result.end = !(scan_sequence->current < scan_sequence->length);
    result.current = scan_sequence->current;
    if (scan_sequence->since_last_write == 1) {
        result.write = true;
        scan_sequence->since_last_write = config->writes;
    } else {
        result.write = false;
        scan_sequence->since_last_write--;
    }
    scan_sequence->current++;
    return result;
}

typedef struct { 
    size_t generated; 
    size_t length;
    size_t max_length;
    size_t since_last_write; 
} RandomSequence;
SequenceResult RandomSequence_next(Arguments *config, AnySequence sequence) {
    RandomSequence *random_sequence = (RandomSequence *) sequence;
    SequenceResult result;
    result.end = !(random_sequence->generated < random_sequence->length);
    result.current = random_index(random_sequence->max_length);
    if (random_sequence->since_last_write == 1) {
        result.write = true;
        random_sequence->since_last_write = config->writes;
    } else {
        result.write = false;
        random_sequence->since_last_write--;
    }
    random_sequence->generated++;
    return result;
}

// MAX LENGTHS
typedef size_t (*max_length_t)(Arguments *config, AnySystem, AnyObject);
size_t fib_max_length(Arguments *config, AnySystem system, AnyObject object) {
    return config->size;
}

// EXECUTION
typedef void (*execution_t)(Arguments *, AnySystem, AnyObject, AnySequence, sequence_t);
void normil_fib_execution(Arguments *config, AnySystem system, AnyObject object, AnySequence sequence, sequence_t next) {
    uint64_t *data = (uint64_t *) object;    
    uint64_t *sum = 0;
    SequenceResult result;
    while (true) {
        result = next(config, sequence);        
        if (result.end) {
            printf("  * end at index %lu\n", result.current);
            break;
        }        
        printf("  * %s fib[%lu]\n", result.write ? "write to" : "read from", result.current);
        if (result.write) {
            data[result.current] = random_int(1000);
        } else {
            sum += data[result.current];
        }
    };
}
void ufo_fib_execution(Arguments *config, AnySystem system, AnyObject object, AnySequence sequence, sequence_t next) {
    uint64_t *data = (uint64_t *) object;
    uint64_t *sum = 0;
    SequenceResult result;
    while (true) {
        result = next(config, sequence);
        if (result.end) {
            break;
        }
        printf("  * %s fib[%lu]\n", result.write ? "write to" : "read from", result.current);
        if (result.write) {
            data[result.current] = random_int(1000);
        } else {
            sum += data[result.current];
        }
    };
}
void ny_fib_execution(Arguments *config, AnySystem system, AnyObject object, AnySequence sequence, sequence_t next) {
    printf("ny_fib_execution not implemented!\n");
    exit(43); 
}

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
    config.writes = 0; // 0 for none
    config.seed = 42;

    // Parse arguments
    static char doc[] = "UFO performance benchmark utility.";
    static char args_doc[] = "";
    static struct argp_option options[] = {
        {"benchmark",       'b', "BENCHMARK",      0,  "Benchmark (populate function) to run: seq, fib, postgres, or bzip"},
        {"implementation",  'i', "IMPL",           0,  "Implementation to run: ufo , ny, normil"},
        {"pattern",         'p', "FILE",           0,  "Read pattern: scan, random"},
        {"sample-size",     'n', "FILE",           0,  "How many elements to read from vector: zero for all"},
        {"writes",          'w', "N%%",            0,  "One write will occur once for every N%% reads, zero for read-only"},
        {"size",            's', "#B",             0,  "Vector size (applicable for fib and seq)"},        
        {"file",            'f', "FILE",           0,  "Input file (applicable for bzip)"},
        {"min-load",        'm', "#B",             0,  "Min load count for ufo"},
        {"high-water-mark", 'h', "#B",             0,  "High water mark for ufo GC"},
        {"low-water-mark",  'l', "#B",             0,  "Low water mark for ufo GC"},
        {"timing",          't', "FILE",           0,  "Path of CSV output file for time measurements"},        
        {"seed",            'S', "N",              0,  "Random seed, default: 42"},
        { 0 }
    };
    static struct argp argp = { options, parse_opt, args_doc, doc };   
    argp_parse (&argp, argc, argv, 0, 0, &config);

    // Check.
    printf("Benchmark configuration:\n");
    printf("  * benchmark:       %s\n",  config.benchmark      );
    printf("  * implementation:  %s\n",  config.implementation );
    printf("  * pattern:         %s\n",  config.pattern        );
    printf("  * size:            %lu\n", config.size           );
    printf("  * min_load:        %lu\n", config.min_load       );
    printf("  * high_water_mark: %lu\n", config.high_water_mark);
    printf("  * low_water_mark:  %lu\n", config.low_water_mark );
    printf("  * file:            %s\n",  config.file           );
    printf("  * timing:          %s\n",  config.timing         );
    printf("  * writes:          %lu\n", config.writes         );
    printf("  * sample_size:     %lu\n", config.sample_size    );
    printf("  * seed:            %u\n",  config.seed           );

    // Random seed
    srand(config.seed);

    // Setup and teardown;
    printf("System configuration\n");
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

    // Object creation, execution and teardown.
    printf("Object configuration\n");
    object_creation_t object_creation = NULL;
    execution_t execution = NULL;
    object_cleanup_t object_cleanup = NULL;
    max_length_t max_length = NULL;
    if ((strcmp(config.benchmark, "fib") == 0) && (strcmp(config.implementation, "ufo") == 0)) {
        object_creation = ufo_fib_creation;
        execution = ufo_fib_execution;
        object_cleanup = ufo_fib_cleanup;
        max_length = fib_max_length;
    }
    if ((strcmp(config.benchmark, "fib") == 0) && (strcmp(config.implementation, "ny") == 0)) {
        object_creation = ny_fib_creation;
        execution = ny_fib_execution;
        object_cleanup = ny_fib_cleanup;
        max_length = fib_max_length;
    }
    if ((strcmp(config.benchmark, "fib") == 0) && (strcmp(config.implementation, "normil") == 0)) {
        object_creation = normil_fib_creation;
        execution = normil_fib_execution;
        object_cleanup = normil_fib_cleanup;
        max_length = fib_max_length;
    }
    if (object_creation == NULL || object_cleanup == NULL) {
        printf("Unknown benchmark/implementation combination \"%s\"/\"%s\"\n", 
        config.benchmark, config.implementation);
        return 4;
    }

    // System setup
    printf("System setup\n");
    uint64_t system_setup_start_time = current_time_in_ns();
    AnySystem system = system_setup(&config);
    uint64_t system_setup_elapsed_time = current_time_in_ns() - system_setup_start_time;

    // Object creation
    printf("Object creation\n");
    uint64_t object_creation_start_time = current_time_in_ns();
    AnyObject object = object_creation(&config, system);
    uint64_t object_creation_elapsed_time = current_time_in_ns() - object_creation_start_time;

    // Detour: sequence selection    
    printf("Index sequence configuration\n");
    AnySequence sequence = NULL;
    sequence_t next = NULL;
    size_t max_sequence_length = max_length(&config, system, object);
    size_t sequence_length = 
        (config.sample_size != 0 && max_sequence_length > config.sample_size) 
        ? config.sample_size : max_sequence_length;
    if (strcmp(config.pattern, "scan") == 0) {
        ScanSequence *scan_sequence = malloc(sizeof(ScanSequence));
        scan_sequence->current = 0;
        scan_sequence->length = sequence_length;
        scan_sequence->since_last_write = config.writes;
        sequence = (AnySequence) scan_sequence;
        next = &ScanSequence_next;
    }
    if (strcmp(config.pattern, "random") == 0) {
        RandomSequence *random_sequence = malloc(sizeof(RandomSequence));
        random_sequence->generated = 0;
        random_sequence->length = sequence_length;
        random_sequence->max_length = max_sequence_length;
        random_sequence->since_last_write = config.writes;
        sequence = (AnySequence) random_sequence;
        next = &RandomSequence_next;
    }
    if (sequence == NULL) {
        printf("Unknown sequence pattern \"%s\"\n", config.pattern);
        return 5;
    }

    // Execution
    printf("Execution\n");
    uint64_t execution_start_time = current_time_in_ns();
    execution(&config, system, object, sequence, next);
    uint64_t execution_elapsed_time = current_time_in_ns() - execution_start_time;

    // Object cleanup
    printf("Object cleanup\n");
    uint64_t object_cleanup_start_time = current_time_in_ns();
    object_cleanup(&config, system, object);
    uint64_t object_cleanup_elapsed_time = current_time_in_ns() - object_cleanup_start_time;
    
    // System teardown
    printf("System teardown\n");
    uint64_t system_teardown_start_time = current_time_in_ns();
    system_teardown(&config, system);
    uint64_t system_teardown_elapsed_time = current_time_in_ns() - system_teardown_start_time;

    // Output (TODO: to CSV)
    FILE *output_stream;
    if (!file_exists(config.timing)) {
        output_stream = fopen(config.timing, "w");
        fprintf(output_stream, 
                "benchmark,"
               "implementation,"
               "pattern,"
               "size,"
               "system_setup_time,"
               "object_creation_time,"
               "execution_time,"
               "object_cleanup_time,"
               "system_teardown_time\n");
    } else {
        output_stream = fopen(config.timing, "a");
    }

    fprintf(output_stream,
        "%s,%s,%s,%lu,%lu,%lu,%lu,%lu,%lu\n",
        config.benchmark,
        config.implementation,
        config.pattern,
        sequence_length,
        system_setup_elapsed_time,
        object_creation_elapsed_time,
        execution_elapsed_time,
        object_cleanup_elapsed_time,
        system_teardown_elapsed_time);

    printf("Results:\n");
    printf("  * system_setup:    %12luns\n", system_setup_elapsed_time);
    printf("  * object_creation: %12luns\n", object_creation_elapsed_time);
    printf("  * execution:       %12luns\n", execution_elapsed_time);
    printf("  * object_cleanup:  %12luns\n", object_cleanup_elapsed_time);
    printf("  * object_teardown: %12luns\n", system_teardown_elapsed_time);

    // Various cleanup
    free(sequence);

}