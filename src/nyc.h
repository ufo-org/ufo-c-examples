#pragma once

#include "bench.h"

void *ny_setup(Arguments *config);
void ny_teardown(Arguments *config, AnySystem system);
size_t ny_max_length(Arguments *config, AnySystem system, AnyObject object);

void *ny_fib_creation(Arguments *config, AnySystem system);
void *ny_bzip_creation(Arguments *config, AnySystem system);
void *ny_seq_creation(Arguments *config, AnySystem system);
void *ny_psql_creation(Arguments *config, AnySystem system);
void *ny_mmap_creation(Arguments *config, AnySystem system);
void *ny_col_creation(Arguments *config, AnySystem system);

void ny_fib_cleanup(Arguments *config, AnySystem system, AnyObject object);
void ny_bzip_cleanup(Arguments *config, AnySystem system, AnyObject object);
void ny_seq_cleanup(Arguments *config, AnySystem system, AnyObject object);
void ny_psql_cleanup(Arguments *config, AnySystem system, AnyObject object);
void ny_mmap_cleanup(Arguments *config, AnySystem system, AnyObject object);
void ny_col_cleanup(Arguments *config, AnySystem system, AnyObject object);

void ny_fib_execution(Arguments *config, AnySystem system, AnyObject object, AnySequence sequence, sequence_t next, volatile int64_t *oubliette);
void ny_bzip_execution(Arguments *config, AnySystem system, AnyObject object, AnySequence sequence, sequence_t next, volatile int64_t *oubliette);
void ny_seq_execution(Arguments *config, AnySystem system, AnyObject object, AnySequence sequence, sequence_t next, volatile int64_t *oubliette);
void ny_psql_execution(Arguments *config, AnySystem system, AnyObject object, AnySequence sequence, sequence_t next, volatile int64_t *oubliette);
void ny_mmap_execution(Arguments *config, AnySystem system, AnyObject object, AnySequence sequence, sequence_t next, volatile int64_t *oubliette);
void ny_col_execution(Arguments *config, AnySystem system, AnyObject object, AnySequence sequence, sequence_t next, volatile int64_t *oubliette);