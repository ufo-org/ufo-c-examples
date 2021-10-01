#pragma once

#include "bench.h"

void *toronto_setup(Arguments *config);
void toronto_teardown(Arguments *config, AnySystem system);
size_t toronto_max_length(Arguments *config, AnySystem system, AnyObject object);

void *toronto_fib_creation(Arguments *config, AnySystem system);
void *toronto_bzip_creation(Arguments *config, AnySystem system);
void *toronto_seq_creation(Arguments *config, AnySystem system);
void *toronto_psql_creation(Arguments *config, AnySystem system);
void *toronto_mmap_creation(Arguments *config, AnySystem system);

void toronto_fib_cleanup(Arguments *config, AnySystem system, AnyObject object);
void toronto_bzip_cleanup(Arguments *config, AnySystem system, AnyObject object);
void toronto_seq_cleanup(Arguments *config, AnySystem system, AnyObject object);
void toronto_psql_cleanup(Arguments *config, AnySystem system, AnyObject object);
void toronto_mmap_cleanup(Arguments *config, AnySystem system, AnyObject object);

void toronto_fib_execution(Arguments *config, AnySystem system, AnyObject object, AnySequence sequence, sequence_t next, volatile int64_t *oubliette);
void toronto_bzip_execution(Arguments *config, AnySystem system, AnyObject object, AnySequence sequence, sequence_t next, volatile int64_t *oubliette);
void toronto_seq_execution(Arguments *config, AnySystem system, AnyObject object, AnySequence sequence, sequence_t next, volatile int64_t *oubliette);
void toronto_psql_execution(Arguments *config, AnySystem system, AnyObject object, AnySequence sequence, sequence_t next, volatile int64_t *oubliette);
void toronto_mmap_execution(Arguments *config, AnySystem system, AnyObject object, AnySequence sequence, sequence_t next, volatile int64_t *oubliette);