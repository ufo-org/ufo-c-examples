#pragma once

#include "bench.h"

void *ufo_setup(Arguments *config);
void ufo_teardown(Arguments *config, AnySystem system);

void *ufo_fib_creation(Arguments *config, AnySystem system);
void *ufo_bzip_creation(Arguments *config, AnySystem system);
void *ufo_seq_creation(Arguments *config, AnySystem system);
void *ufo_psql_creation(Arguments *config, AnySystem system);
void *ufo_mmap_creation(Arguments *config, AnySystem system);
void *ufo_col_creation(Arguments *config, AnySystem system);

void ufo_fib_cleanup(Arguments *config, AnySystem system, AnyObject object);
void ufo_bzip_cleanup(Arguments *config, AnySystem system, AnyObject object);
void ufo_seq_cleanup(Arguments *config, AnySystem system, AnyObject object);
void ufo_psql_cleanup(Arguments *config, AnySystem system, AnyObject object);
void ufo_mmap_cleanup(Arguments *config, AnySystem system, AnyObject object);
void ufo_col_cleanup(Arguments *config, AnySystem system, AnyObject object);

