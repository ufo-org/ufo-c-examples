#pragma once

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "ufo_c/target/ufo_c.h"

typedef int(*char_map_t)(int);

typedef struct {
    size_t size;
    char *data;
} MMap;

char *mmap_new(char *filename, size_t *size);
int32_t mmap_populate(void* user_data, uintptr_t start, uintptr_t end, unsigned char* target_bytes);

MMap *MMap_ufo_new(UfoCore *ufo_system, char *filename, char_map_t map_f, bool read_only, size_t min_load_count);
void MMap_ufo_free(UfoCore *ufo_system, MMap *ptr);

MMap *MMap_normil_new(char *filename, char_map_t map_f);
void MMap_normil_free(MMap *ptr);