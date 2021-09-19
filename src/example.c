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
    UfoObj ufo_object = ufo_new_no_prototype(
        ufo_system,  
        /* header size */ 0,  
        /* element size */ strideOf(Obj),
        /* min elements loaded at a time */ MIN_LOAD_COUNT,
        /* read-only */ false, 
        /* size */ n, 
        /* data */ NULL,
        /* populate function */ Obj_populate
    );
    if (ufo_is_error(&ufo_object)) {
        fprintf(stderr, "Cannot create UFO object"); // TODO
        return NULL;
    }
    return (Obj *) ufo_header_ptr(&ufo_object);
}

void Obj_free(UfoCore *ufo_system, Obj *ptr) {
    UfoObj ufo_object = ufo_get_by_address(ufo_system, ptr);
    if (ufo_is_error(&ufo_object)) {
        fprintf(stderr, "Cannot free %p: not a UFO object.", ptr); // TODO
        return;
    }
    ufo_free(ufo_object);
}

int main(int argc, char *argv[]) {
    UfoCore ufo_system = ufo_new_core("/tmp/ufos/", HIGH_WATER_MARK, LOW_WATER_MARK);
    size_t size = HIGH_WATER_MARK * 2;

    Obj *objects = Obj_new(&ufo_system, size);
    for (size_t i = 0; i < size; i++) {
        printf("%li -> %li\n", i, objects[i].id);

        objects[i].id = objects[i].id + 1;
    }

    Obj_free(&ufo_system, objects);
    ufo_core_shutdown(ufo_system);
}