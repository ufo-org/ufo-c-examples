#include <stdint.h>

#include "ufo_c/target/ufo_c.h"

#include <bzlib.h>

typedef struct {
    size_t size;
    char *data;
} BZip2;

BZip2 *BZip2_ufo_new(UfoCore *ufo_system, char *filename, bool read_only, size_t min_load_count);
void BZip2_ufo_free(UfoCore *ufo_system, BZip2 *object);

BZip2 *BZip2_normil_new(char *filename);
void BZip2_normil_free(BZip2 *object);