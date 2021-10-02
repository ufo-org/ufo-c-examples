#pragma once

#include "bench.h"

void *normil_setup(Arguments *config);
void normil_teardown(Arguments *config, AnySystem system);

void *normil_fib_creation(Arguments *config, AnySystem system);
void *normil_bzip_creation(Arguments *config, AnySystem system);
void *normil_seq_creation(Arguments *config, AnySystem system);
void *normil_psql_creation(Arguments *config, AnySystem system);
void *normil_mmap_creation(Arguments *config, AnySystem system);
void *normil_col_creation(Arguments *config, AnySystem system);

void normil_fib_cleanup(Arguments *config, AnySystem system, AnyObject object);
void normil_bzip_cleanup(Arguments *config, AnySystem system, AnyObject object);
void normil_seq_cleanup(Arguments *config, AnySystem system, AnyObject object);
void normil_psql_cleanup(Arguments *config, AnySystem system, AnyObject object);
void normil_mmap_cleanup(Arguments *config, AnySystem system, AnyObject object);
void normil_col_cleanup(Arguments *config, AnySystem system, AnyObject object);
