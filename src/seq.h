#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "ufo_c/target/ufo_c.h"
#include "new_york/target/nyc.h"

// TODO MIN_LOAD_COUNT as parameter

typedef struct {
    size_t from;
    size_t to;
    size_t by;
    size_t length;
} Seq;

int32_t seq_populate(void* user_data, uintptr_t start, uintptr_t end, unsigned char* target_bytes);

int64_t *seq_ufo_from_length(UfoCore *ufo_system, size_t from, size_t length, size_t by, bool read_only, size_t min_load_count);
int64_t *seq_ufo_new(UfoCore *ufo_system, size_t from, size_t to, size_t by, bool read_only, size_t min_load_count);
int64_t *seq_ufo_from_Seq(UfoCore *ufo_system, Seq data, bool read_only, size_t min_load_count);
void seq_ufo_free(UfoCore *ufo_system, int64_t *ptr);

int64_t *seq_normil_new(size_t from, size_t to, size_t by);
int64_t *seq_normil_from_length(size_t from, size_t length, size_t by);
int64_t *seq_normil_from_Seq(Seq data);
void seq_normil_free(int64_t *ptr);

Borough *seq_nyc_from_length(NycCore *system, size_t from, size_t length, size_t by, size_t min_load_count);
Borough *seq_nyc_new(NycCore *system, size_t from, size_t to, size_t by, size_t min_load_count);
Borough *seq_nyc_from_Seq(NycCore *system, Seq data, size_t min_load_count);
void seq_nyc_free(NycCore *system, Borough *ptr);