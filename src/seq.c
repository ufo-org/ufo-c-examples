#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "seq.h"

int32_t seq_populate(void* user_data, uintptr_t start, uintptr_t end, unsigned char* target_bytes) {

    Seq *data = (Seq *) user_data;
    int64_t* target = (int64_t *)  target_bytes;

    for (size_t i = 0; i < end - start; i++) {
        target[i] = data->from + data->by * (i + start);
    }

    return 0;
}

int64_t *seq_ufo_from_length(UfoCore *ufo_system, size_t from, size_t length, size_t by, bool read_only, size_t min_load_count) {
    Seq data;
    data.from = from; 
    data.to = (length - 1) * by + from;
    data.length = length;
    data.by = by;
    return seq_ufo_from_Seq(ufo_system, data, read_only, min_load_count);
}

int64_t *seq_ufo_new(UfoCore *ufo_system, size_t from, size_t to, size_t by, bool read_only, size_t min_load_count) {
    Seq data;
    data.from = from; 
    data.to = to;
    data.length = (to - from) / by + 1;
    data.by = by;
    return seq_ufo_from_Seq(ufo_system, data, read_only, min_load_count);
}

int64_t *seq_ufo_from_Seq(UfoCore *ufo_system, Seq data, bool read_only, size_t min_load_count) {
    UfoParameters parameters;
    parameters.header_size = 0;
    parameters.element_size = strideOf(int64_t);
    parameters.element_ct = data.length;
    parameters.min_load_ct = min_load_count;
    parameters.read_only = false;
    parameters.populate_data = &data; // populate data is copied by UFO, so this should be fine.
    parameters.populate_fn = seq_populate;

    UfoObj ufo_object = ufo_new_object(ufo_system, &parameters);
    if (ufo_is_error(&ufo_object)) {
        fprintf(stderr, "Cannot create UFO object.\n");
        return NULL;
    }

    int64_t *pointer = ufo_header_ptr(&ufo_object);
    return pointer;
}

void seq_ufo_free(UfoCore *ufo_system, int64_t *ptr) {
    UfoObj ufo_object = ufo_get_by_address(ufo_system, ptr);
    if (ufo_is_error(&ufo_object)) {
        fprintf(stderr, "Cannot free %p: not a UFO object.\n", ptr);
        return;
    }

    // No need to free parameters.

    ufo_free(ufo_object);
}

int64_t *seq_normil_new(size_t from, size_t to, size_t by) {
    Seq data;
    data.from = from; 
    data.to = to;
    data.length = (to - from) / by + 1;
    data.by = by;
    return seq_normil_from_Seq(data);
}
int64_t *seq_normil_from_length(size_t from, size_t length, size_t by) {
    Seq data;
    data.from = from; 
    data.to = (length - 1) * by + from;
    data.length = length;
    data.by = by;
    return seq_normil_from_Seq(data);
}
int64_t *seq_normil_from_Seq(Seq data) {
    int64_t *target = (int64_t *) malloc(sizeof(int64_t) * data.length);
    seq_populate(&data, 0, data.length, (unsigned char *) target);
    return target;
}
void seq_normil_free(int64_t *ptr) {
    free(ptr);
}

Borough *seq_nyc_from_length(NycCore *system, size_t from, size_t length, size_t by, size_t min_load_count) {
    Seq data;
    data.from = from; 
    data.to = (length - 1) * by + from;
    data.length = length;
    data.by = by;
    return seq_nyc_from_Seq(system, data, min_load_count);
}
Borough *seq_nyc_new(NycCore *system, size_t from, size_t to, size_t by, size_t min_load_count) {
    Seq data;
    data.from = from; 
    data.to = to;
    data.length = (to - from) / by + 1;
    data.by = by;
    return seq_nyc_from_Seq(system, data, min_load_count);
}
Borough *seq_nyc_from_Seq(NycCore *system, Seq data, size_t min_load_count) {
    BoroughParameters parameters;
    parameters.header_size = 0;
    parameters.element_size = strideOf(int64_t);
    parameters.element_ct = data.length;
    parameters.min_load_ct = min_load_count;
    parameters.populate_data = &data; // populate data is copied by UFO, so this should be fine.
    parameters.populate_fn = seq_populate;

    Borough *object = (Borough *) malloc(sizeof(Borough));
    *object = nyc_new_borough(system, &parameters);
    if (borough_is_error(object)) {
        fprintf(stderr, "Cannot create NYC object.\n");
        return NULL;
    }

    return object;
}
void seq_nyc_free(NycCore *system, Borough *object) {    
    borough_free(*object);
    free(object);
}


Village *seq_toronto_from_length(TorontoCore *system, size_t from, size_t length, size_t by, size_t min_load_count) {
    Seq data;
    data.from = from; 
    data.to = (length - 1) * by + from;
    data.length = length;
    data.by = by;
    return seq_toronto_from_Seq(system, data, min_load_count);
}
Village *seq_toronto_new(TorontoCore *system, size_t from, size_t to, size_t by, size_t min_load_count) {
    Seq data;
    data.from = from; 
    data.to = to;
    data.length = (to - from) / by + 1;
    data.by = by;
    return seq_toronto_from_Seq(system, data, min_load_count);
}
Village *seq_toronto_from_Seq(TorontoCore *system, Seq data, size_t min_load_count) {
    VillageParameters parameters;
    parameters.header_size = 0;
    parameters.element_size = strideOf(int64_t);
    parameters.element_ct = data.length;
    parameters.min_load_ct = min_load_count;
    parameters.populate_data = &data; // populate data is copied by UFO, so this should be fine.
    parameters.populate_fn = seq_populate;

    Village *object = (Village *) malloc(sizeof(Village));
    *object = toronto_new_village(system, &parameters);
    if (village_is_error(object)) {
        fprintf(stderr, "Cannot create NYC object.\n");
        return NULL;
    }

    return object;
}
void seq_toronto_free(TorontoCore *system, Village *object) {    
    village_free(*object);
    free(object);
}