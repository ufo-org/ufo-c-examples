#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "ufo_c/target/ufo_c.h"

#define HIGH_WATER_MARK (2L * 1024 * 1024 * 1024)
#define LOW_WATER_MARK  (1L * 1024 * 1024 * 1024)
#define MIN_LOAD_COUNT  (4L * 1024)

typedef struct {
    size_t id;
} Obj;

static int32_t Obj_populate(void* user_data, uintptr_t start, uintptr_t end, unsigned char* target) {
    Obj *objects = (Obj *) target;
    for (size_t i = 0; i < end - start; i++) {
        Obj object;
        object.id = start + i;
        objects[i] = object;
    }
    return 0;
}

Obj *Obj_new(UfoCore *ufo_system, size_t n) {

    UfoParameters parameters;
    parameters.header_size = 0;
    parameters.element_size = strideOf(Obj);
    parameters.element_ct = n;
    parameters.min_load_ct = MIN_LOAD_COUNT;
    parameters.read_only = false;
    parameters.populate_data = NULL;
    parameters.populate_fn = Obj_populate;

    UfoObj ufo_object = ufo_new_object(ufo_system, &parameters);

    if (ufo_is_error(&ufo_object)) {
        fprintf(stderr, "Cannot create UFO object.\n");
        return NULL;
    }

    return (Obj *) ufo_header_ptr(&ufo_object);
}

void Obj_free(UfoCore *ufo_system, Obj *ptr) {
    UfoObj ufo_object = ufo_get_by_address(ufo_system, ptr);
    if (ufo_is_error(&ufo_object)) {
        fprintf(stderr, "Cannot free %p: not a UFO object.\n", ptr);
        return;
    }
    ufo_free(ufo_object);
}

int main(int argc, char *argv[]) {
    UfoCore ufo_system = ufo_new_core("/tmp/ufos/", HIGH_WATER_MARK, LOW_WATER_MARK);
        if (ufo_core_is_error(&ufo_system)) {
        exit(1);
    }

    size_t size = HIGH_WATER_MARK * 2;

    Obj *objects = Obj_new(&ufo_system, size);
    if (objects == NULL) {
        exit(2);
    }
    for (size_t i = 0; i < size; i++) {
        printf("%li -> %li\n", i, objects[i].id);

        objects[i].id = objects[i].id + 1;
    }

    Obj_free(&ufo_system, objects);
    ufo_core_shutdown(ufo_system);
}