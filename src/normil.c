#include "ufo.h"

#include <ctype.h>

#include "seq.h"
#include "fib.h"
#include "bzip.h"
#include "mmap.h"
#include "postgres.h"
#include "col.h"

#include "logging.h"

void *normil_setup(Arguments *config) {
    return NULL;
}

void normil_teardown(Arguments *config, AnySystem system) {
    /* empty */
}

// Fib
void *normil_fib_creation(Arguments *config, AnySystem system) {
    return (void *) normil_fib_new(config->size);
}
void normil_fib_cleanup(Arguments *config, AnySystem system, AnyObject object) {
    normil_fib_free(object);
}

// Bzip
void *normil_bzip_creation(Arguments *config, AnySystem system) {
    return (void *) BZip2_normil_new(config->file);
}
void normil_bzip_cleanup(Arguments *config, AnySystem system, AnyObject object) {
    BZip2_normil_free(object);
}

// Seq
void *normil_seq_creation(Arguments *config, AnySystem system) {
    return (void *) seq_normil_from_length(1, config->size, 2);
}
void normil_seq_cleanup(Arguments *config, AnySystem system, AnyObject object) {
    seq_normil_free(object);
}

// Psql
void *normil_psql_creation(Arguments *config, AnySystem system) {
    return (void *) Players_normil_new();
}
void normil_psql_cleanup(Arguments *config, AnySystem system, AnyObject object) {
    Players_normil_free(object);
}

// MMap
void *normil_mmap_creation(Arguments *config, AnySystem system) {
    return (void *) MMap_normil_new(config->file, toupper);
}
void normil_mmap_cleanup(Arguments *config, AnySystem system, AnyObject object) {
    MMap_normil_free(object);
}

// Col
void *normil_col_creation(Arguments *config, AnySystem system) {
    int32_t **matrix = col_source_matrix_new(config->size, COL_COLUMNS_IN_EACH_ROW);
    int32_t *result = col_normil_new(matrix, COL_SELECTED_COLUMN, config->size);
    col_source_matrix_free(matrix, config->size);
    return (void *) result;
}
void normil_col_cleanup(Arguments *config, AnySystem system, AnyObject object) {
    col_normil_free(object);   
}