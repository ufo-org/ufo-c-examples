#include "mmap.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>

#include "logging.h"

#include "ufo_c/target/ufo_c.h"

typedef struct {
    char *source;
    size_t size;
    char_map_t map_f;
} MMapData;

char *mmap_new(char *filename, size_t *size) {
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        return NULL;
    }
    int file_no = fileno(file);
    fseek(file, 0L, SEEK_END);
    *size = ftell(file);
    char *data = (char *) mmap(
        /* addr   */ NULL, 
        /* length */ *size, 
        /* prot   */ PROT_READ | PROT_WRITE, 
        /* flags  */ MAP_PRIVATE, 
        /* fd     */ file_no, 
        /* offset */ 0L
    );
    fclose(file);
    return data;
}

int32_t mmap_populate(void* user_data, uintptr_t start, uintptr_t end, unsigned char* target_bytes) {
    MMapData *mmap = (MMapData *) user_data;
    char *target = (char *) target_bytes;
    size_t length = end - start;
    for (size_t i = 0; i < length; i++) {
        target[i] = mmap->map_f(mmap->source[i]);
    }    
    return 0;
}

MMap *MMap_ufo_new(UfoCore *ufo_system, char *filename, char_map_t map_f, bool read_only, size_t min_load_count) {
    size_t size;
    char *data = mmap_new(filename, &size);
    if (data == NULL) {
        perror("ERROR");
        REPORT("Cannot open file %s", filename);
        return NULL;
    }

    MMapData *mmap = malloc(sizeof(MMapData));
    mmap->size = size;
    mmap->source = data;
    mmap->map_f = map_f;

    UfoParameters parameters;
    parameters.header_size = 0;
    parameters.element_size = strideOf(char);
    parameters.element_ct = size;
    parameters.min_load_ct = min_load_count;
    parameters.read_only = read_only;
    parameters.populate_data = mmap;
    parameters.populate_fn = mmap_populate;

    UfoObj ufo_object = ufo_new_object(ufo_system, &parameters);
    if (ufo_is_error(&ufo_object)) {
        fprintf(stderr, "Cannot create UFO object.\n");
        return NULL;
    }

    MMap *mmap_object = malloc(sizeof(MMap));
    mmap_object->data = ufo_header_ptr(&ufo_object);
    mmap_object->size = size;
    return mmap_object;
}

void MMap_ufo_free(UfoCore *ufo_system, MMap *mmap_object) {
    UfoObj ufo_object = ufo_get_by_address(ufo_system, mmap_object->data);
    if (ufo_is_error(&ufo_object)) {
        fprintf(stderr, "Cannot free %p: not a UFO object.\n", mmap_object->data);
        return;
    }

    UfoParameters parameters;
    int result = ufo_get_params(ufo_system, &ufo_object, &parameters);
    if (result < 0) {
        fprintf(stderr, "Unable to access UFO parameters.\n");
        ufo_free(ufo_object);
        return;
    }
    
    MMapData *mmap = (MMapData *) parameters.populate_data;
    munmap(mmap->source, mmap->size);    
    free(mmap);
    free(mmap_object);
    ufo_free(ufo_object);
}

MMap *MMap_normil_new(char *filename, char_map_t map_f) {
    size_t size;
    char *data = mmap_new(filename, &size);

    if (data == NULL) {
        perror("ERROR");
        REPORT("Cannot open file %s\n", filename);
        return NULL;
    }

    for (size_t i = 0; i < size; i++) {
        printf("i=%lu\n", i);
        printf("i=%lu, data[%lu]=%c f=%c\n", i, i, data[i], map_f(data[i]));
        data[i] = map_f(data[i]);
    }

    MMap *mmap_object = malloc(sizeof(MMap));
    mmap_object->data = data;
    mmap_object->size = size;
    return mmap_object;
}

void MMap_normil_free(MMap *mmap_object) {    
    munmap(mmap_object->data, mmap_object->size);
    free(mmap_object);
}