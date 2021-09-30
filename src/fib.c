#include "fib.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "ufo_c/target/ufo_c.h"
#include "new_york/target/nyc.h"

typedef struct {
    uint64_t *self;
} Fib;

int32_t fib_populate(void* user_data, uintptr_t start, uintptr_t end, unsigned char* target_bytes) {
    Fib *data = (Fib *) user_data;
    uint64_t *target = (uint64_t *) target_bytes;

    target[0] = (start == 0) ? 1 : data->self[start-1] + data->self[start-2];
    target[1] = (start == 0) ? 1 : target[0] + data->self[start-1];

    size_t length = end - start;
    for (size_t i = 2; i < length; i++) {
        target[i] = target[i - 1] + target[i - 2];
    }
    
    return 0;
}

uint64_t *ufo_fib_new(UfoCore *ufo_system, size_t n, bool read_only, size_t min_load_count) {

    Fib *data = (Fib *) malloc(sizeof(Fib));
    data->self = NULL;

    UfoParameters parameters;
    parameters.header_size = 0;
    parameters.element_size = strideOf(uint64_t);
    parameters.element_ct = n;
    parameters.min_load_ct = min_load_count;
    parameters.read_only = read_only;
    parameters.populate_data = data;
    parameters.populate_fn = fib_populate;

    UfoObj ufo_object = ufo_new_object(ufo_system, &parameters);

    if (ufo_is_error(&ufo_object)) {
        fprintf(stderr, "Cannot create UFO object.\n");
        return NULL;
    }

    uint64_t *pointer = ufo_header_ptr(&ufo_object);
    data->self = pointer;   
    return pointer;
}

void ufo_fib_free(UfoCore *ufo_system, uint64_t *ptr) {
    UfoObj ufo_object = ufo_get_by_address(ufo_system, ptr);
    if (ufo_is_error(&ufo_object)) {
        fprintf(stderr, "Cannot free %p: not a UFO object.\n", ptr);
        return;
    }
    UfoParameters parameters;
    int result = ufo_get_params(ufo_system, &ufo_object, &parameters);
    if (result < 0) {
        fprintf(stderr, "Unable to access UFO parameters.\n");
        ufo_free(ufo_object);
        return;
    }
    free(parameters.populate_data);
    ufo_free(ufo_object);
}

uint64_t *normil_fib_new(size_t n) {
    uint64_t *target = (uint64_t *) malloc(sizeof(uint64_t) * n);

    target[0] = 1;
    target[1] = 1;

    for (size_t i = 2; i < n; i++) {
        target[i] = target[i - 1] + target[i - 2];
    }
    
    return target;
}

void normil_fib_free(uint64_t * ptr) {
    free(ptr);
}

Borough *nyc_fib_new(NycCore *system, size_t n, size_t min_load_count) {

    Fib *data = (Fib *) malloc(sizeof(Fib));
    data->self = NULL;

    BoroughParameters parameters;
    parameters.header_size = 0;
    parameters.element_size = strideOf(uint64_t);
    parameters.element_ct = n;
    parameters.min_load_ct = min_load_count;
    parameters.populate_data = data;
    parameters.populate_fn = fib_populate;

    Borough *object = (Borough *) malloc(sizeof(Borough));
    *object = nyc_new_borough(system, &parameters);

    if (borough_is_error(object)) {
        fprintf(stderr, "Cannot create NYC object.\n");
        return NULL;
    }

    return object;
}

void nyc_fib_free(NycCore *system, Borough *object) {    
    BoroughParameters parameters;
    borough_params(object, &parameters);    
    free(parameters.populate_data);
    borough_free(*object);
    free(object);
}