#include "ufo.h"

#include <ctype.h>

#include "seq.h"
#include "fib.h"
#include "bzip.h"
#include "mmap.h"
#include "postgres.h"

#include "logging.h"

void *ny_setup(Arguments *config) {
    REPORT("ny_setup unimplemented!\n");
    exit(99);
}
void ny_teardown(Arguments *config, AnySystem system) {
    printf("ny_teardown unimplemented!\n");
    exit(99);
}

// Fibonacci
void *ny_fib_creation(Arguments *config, AnySystem system) {
    REPORT("ny_fib_creation unimplemented!\n");
    exit(99);
}
void ny_fib_cleanup(Arguments *config, AnySystem system, AnyObject object) {
    REPORT("ny_fib_cleanup unimplemented!\n");
    exit(99);
}
void ny_fib_execution(Arguments *config, AnySystem system, AnyObject object, AnySequence sequence, sequence_t next, volatile int64_t *oubliette) {
    REPORT("ny_fib_execution not implemented!\n");
    exit(99); 
}

// BZip
void *ny_bzip_creation(Arguments *config, AnySystem system) {
    REPORT("ny_bzip_creation unimplemented!\n");
    exit(99);
}
void ny_bzip_cleanup(Arguments *config, AnySystem system, AnyObject object) {
    REPORT("ny_bzip_cleanup unimplemented!\n");
    exit(99);
}
void ny_bzip_execution(Arguments *config, AnySystem system, AnyObject object, AnySequence sequence, sequence_t next, volatile int64_t *oubliette) {
    REPORT("ny_bzip_execution not implemented!\n");
    exit(99); 
}

// Seq
void *ny_seq_creation(Arguments *config, AnySystem system) {
    REPORT("ny_seq_creation unimplemented!\n");
    exit(99);
}
void ny_seq_cleanup(Arguments *config, AnySystem system, AnyObject object) {
    REPORT("ny_seq_cleanup unimplemented!\n");
    exit(99);
}
void ny_seq_execution(Arguments *config, AnySystem system, AnyObject object, AnySequence sequence, sequence_t next, volatile int64_t *oubliette) {
    REPORT("ny_fib_execution not implemented!\n");
    exit(99); 
}

// Psql
void *ny_psql_creation(Arguments *config, AnySystem system) {
    REPORT("ny_psql_creation unimplemented!\n");
    exit(99);
}
void ny_psql_cleanup(Arguments *config, AnySystem system, AnyObject object) {
    REPORT("ny_psql_cleanup unimplemented!\n");
    exit(99);
}
void ny_psql_execution(Arguments *config, AnySystem system, AnyObject object, AnySequence sequence, sequence_t next, volatile int64_t *oubliette) {
    REPORT("ny_psql_execution not implemented!\n");
    exit(99); 
}


// MMap
void *ny_mmap_creation(Arguments *config, AnySystem system) {
    REPORT("ny_mmap_creation unimplemented!\n");
    exit(99);
}
void ny_mmap_cleanup(Arguments *config, AnySystem system, AnyObject object) {
    REPORT("ny_mmap_cleanup unimplemented!\n");
    exit(99);
}
void ny_mmap_execution(Arguments *config, AnySystem system, AnyObject object, AnySequence sequence, sequence_t next, volatile int64_t *oubliette) {
    REPORT("ny_mmap_execution unimplemented!\n");
    exit(99);
}
