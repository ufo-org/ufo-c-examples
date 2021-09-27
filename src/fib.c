#include "fib.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "ufo_c/target/ufo_c.h"

#define HIGH_WATER_MARK (2L * 1024 * 1024 * 1024)
#define LOW_WATER_MARK  (1L * 1024 * 1024 * 1024)
#define MIN_LOAD_COUNT  (4L * 1024)

typedef struct {
    uint64_t *self;
} Fib;

static int32_t fib_populate(void* user_data, uintptr_t start, uintptr_t end, unsigned char* target_bytes) {
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

uint64_t *fib_new(UfoCore *ufo_system, size_t n) {

    Fib *data = (Fib *) malloc(sizeof(Fib));
    data->self = NULL;

    UfoParameters parameters;
    parameters.header_size = 0;
    parameters.element_size = strideOf(uint64_t);
    parameters.element_ct = n;
    parameters.min_load_ct = MIN_LOAD_COUNT;
    parameters.read_only = false;
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

void fib_free(UfoCore *ufo_system, uint64_t *ptr) {
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

int main(int argc, char *argv[]) {
    UfoCore ufo_system = ufo_new_core("/tmp/", HIGH_WATER_MARK, LOW_WATER_MARK);
    if (ufo_core_is_error(&ufo_system)) {
        exit(1);
    }

    size_t size = 100000;
    uint64_t *fib = fib_new(&ufo_system, size);
    if (fib == NULL) {
        exit(1);
    }
    for (size_t i = 0; i < size; i++) {
        printf("%lu -> %lu\n", i, fib[i]);
    }

    fib_free(&ufo_system, fib);
    ufo_core_shutdown(ufo_system);
}