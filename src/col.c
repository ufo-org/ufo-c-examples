#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "logging.h"

#include "col.h"

int32_t col_populate(void* user_data, uintptr_t start, uintptr_t end, unsigned char* target_bytes) {

    ColumnSpec *data = (ColumnSpec *) user_data;
    int32_t* target = (int32_t *)  target_bytes;

    for (size_t i = 0; i < end - start; i++) {
        target[i] = data->source[i][data->column];
    }

    return 0;
}

int32_t *col_ufo_new(UfoCore *ufo_system, int32_t **source, size_t column, size_t size, bool read_only, size_t min_load_count) {
    printf("col ufo new on %p\n", source);

    ColumnSpec *data = (ColumnSpec *) malloc(sizeof(ColumnSpec));
    data->column = column; 
    data->size = size;
    data->source = source;

    UfoParameters parameters;
    parameters.header_size = 0;
    parameters.element_size = strideOf(int32_t);
    parameters.element_ct = data->size;
    parameters.min_load_ct = min_load_count;
    parameters.read_only = false;
    parameters.populate_data = data;
    parameters.populate_fn = col_populate;

    UfoObj ufo_object = ufo_new_object(ufo_system, &parameters);
    if (ufo_is_error(&ufo_object)) {
        fprintf(stderr, "Cannot create UFO object.\n");
        return NULL;
    }

    int32_t *pointer = ufo_header_ptr(&ufo_object);
    return pointer;
}

void col_ufo_free(UfoCore *ufo_system, int64_t *ptr) {
    UfoObj ufo_object = ufo_get_by_address(ufo_system, ptr);
    if (ufo_is_error(&ufo_object)) {
        fprintf(stderr, "Cannot free %p: not a UFO object.\n", ptr);
        return;
    }

    UfoParameters parameters;
    int result = ufo_get_params(ufo_system, &ufo_object, &parameters);
    if (result < 0) {
        REPORT("Unable to access UFO parameters, so cannot free source matrix\n");
    }
    free(parameters.populate_data);

    ufo_free(ufo_object);
}

int32_t *col_normil_new(int32_t **source, size_t column, size_t size) {
    ColumnSpec data;
    data.column = column; 
    data.size = size;
    data.source = source;

    int32_t *target = (int32_t *) malloc(sizeof(int32_t) * data.size);
    col_populate(&data, 0, data.size, (unsigned char *) target);
    return target;
}

void col_normil_free(int64_t *ptr) {
    free(ptr);
}

Borough *col_nyc_new(NycCore *system, int32_t **source, size_t column, size_t size, size_t min_load_count) {
    ColumnSpec *data = (ColumnSpec *) malloc(sizeof(ColumnSpec));
    data->column = column; 
    data->size = size;
    data->source = source;

    BoroughParameters parameters;
    parameters.header_size = 0;
    parameters.element_size = strideOf(int32_t);
    parameters.element_ct = data->size;
    parameters.min_load_ct = min_load_count;
    parameters.populate_data = data;
    parameters.populate_fn = col_populate;

    Borough *object = (Borough *) malloc(sizeof(Borough));
    *object = nyc_new_borough(system, &parameters);
    if (borough_is_error(object)) {
        fprintf(stderr, "Cannot create NYC object.\n");
        return NULL;
    }
    return object;
}

void col_nyc_free(NycCore *system, Borough *object) {
    BoroughParameters parameters;
    borough_params(object, &parameters);
    free(parameters.populate_data);
    borough_free(*object);
    free(object);
}

Village *col_toronto_new(TorontoCore *system, int32_t **source, size_t column, size_t size, size_t min_load_count) {
    ColumnSpec *data = (ColumnSpec *) malloc(sizeof(ColumnSpec));
    data->column = column; 
    data->size = size;
    data->source = source;

    VillageParameters parameters;
    parameters.header_size = 0;
    parameters.element_size = strideOf(int32_t);
    parameters.element_ct = data->size;
    parameters.min_load_ct = min_load_count;
    parameters.populate_data = data;
    parameters.populate_fn = col_populate;

    Village *object = (Village *) malloc(sizeof(Village));
    *object = toronto_new_village(system, &parameters);
    if (village_is_error(object)) {
        fprintf(stderr, "Cannot create TORONTO object.\n");
        return NULL;
    }
    return object;
}

void col_toronto_free(TorontoCore *system, Village *object) {
    VillageParameters parameters;
    village_params(object, &parameters);
    free(parameters.populate_data);  
    village_free(*object);
    free(object);
}

int32_t **col_source_matrix_new(size_t rows, size_t columns) {
    int32_t **data = (int32_t **) malloc(sizeof(int32_t *) * rows);
    for (size_t row_index = 0; row_index < rows; row_index++) {
        int32_t *row = (int32_t *) malloc(sizeof(int32_t) * columns);
        data[row_index] = row;
        for (size_t column_index = 0; column_index < columns; column_index++) {
            row[column_index] = column_index + 1;
        }        
    }
    return data;
}

void col_source_matrix_free(int32_t** matrix, size_t rows) {
    for (size_t row_index = 0; row_index < rows; row_index++) {
        free(matrix[row_index]);
    }
    free(matrix);
}