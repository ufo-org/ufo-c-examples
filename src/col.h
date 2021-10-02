#pragma once
#include <stdint.h>
#include <stdbool.h>

#include "ufo_c/target/ufo_c.h"
#include "new_york/target/nyc.h"
#include "toronto/target/toronto.h"

#define COL_COLUMNS_IN_EACH_ROW 10
#define COL_SELECTED_COLUMN  7

typedef struct {
    int32_t **source;
    size_t size;
    size_t column;

} ColumnSpec;

int32_t **col_source_matrix_new(size_t rows, size_t columns);
void col_source_matrix_free(int32_t** matrix, size_t rows);

int32_t col_populate(void* user_data, uintptr_t start, uintptr_t end, unsigned char* target_bytes);

int32_t *col_ufo_new(UfoCore *ufo_system, int32_t **source, size_t column, size_t size, bool read_only, size_t min_load_count);
void col_ufo_free(UfoCore *ufo_system, int64_t *ptr);

int32_t *col_normil_new(int32_t **source, size_t column, size_t size);
void col_normil_free(int64_t *ptr);

Borough *col_nyc_new(NycCore *system, int32_t **source, size_t column, size_t size, size_t min_load_count);
void col_nyc_free(NycCore *system, Borough *ptr);

Village *col_toronto_new(TorontoCore *system, int32_t **source, size_t column, size_t size, size_t min_load_count);
void col_toronto_free(TorontoCore *system, Village *object);